#include "connman_manager.h"

#include <iostream>
#include <mutex>

using namespace connman::msg;

// ── Lifecycle & Initialization ──────────────────────────────────────────────

ConnmanManager::ConnmanManager(sdbus::IConnection& conn,
                               Dart_Port_DL events_port)
    : ConnmanManagerProxyHolder(conn),
      net::connman::Manager_proxy(*this->proxy_),
      conn_(conn),
      events_port_(events_port) {
  try {
    registerProxy();
  } catch (const sdbus::Error& e) {
    std::cerr << "Manager registerProxy failed: " << e.getMessage() << "\n";
  }
}

// ── Snapshot / Initial state fetching ───────────────────────────────────────

void ConnmanManager::get_managed_objects() {
  // Fetch the full D-Bus state outside the lock — these are blocking calls
  // and we must not hold the mutex while waiting on D-Bus.
  ConnmanManagerProps mgr_props;
  std::map<std::string, ConnmanTechnologyProps> new_technologies;
  std::map<std::string, ConnmanServiceProps> new_services;

  try {
    mgr_props = extract_manager_props(GetProperties());

    for (const auto& tech : GetTechnologies()) {
      auto p = extract_technology_props(tech.get<0>(), tech.get<1>());
      new_technologies[p.objectPath] = p;
    }

    for (const auto& svc : GetServices()) {
      auto p = extract_service_props(svc.get<0>(), svc.get<1>());
      new_services[p.objectPath] = p;
    }
  } catch (const sdbus::Error& e) {
    std::cerr << "ConnmanManager::get_managed_objects failed: " << e.getName()
              << " - " << e.getMessage() << "\n";
    post_glaze(kError, ConnmanError{ConnmanManagerProxyHolder::kService,
                                    e.getName(), e.getMessage()});
    return;
  }

  // Commit to the object tree and post the initial snapshot atomically so
  // signal handlers cannot interleave their posts with ours.
  std::scoped_lock lock(obj_tree_mutex_);

  technologies_ = std::move(new_technologies);
  services_ = std::move(new_services);

  for (const auto& [path, tech] : technologies_) {
    post_glaze(kTechnologyProps, tech);
  }
  for (const auto& [path, svc] : services_) {
    post_glaze(kServiceProps, svc);
  }
  // Post kManagerProps last so it acts as the "snapshot complete" sentinel.
  // The Dart side awaits this message before returning from connect(); posting
  // it after all kTechnologyProps / kServiceProps messages guarantees that the
  // ReceivePort has already queued the full initial state before connect()
  // unblocks and the caller accesses client.technologies / client.services.
  post_glaze(kManagerProps, mgr_props);
}

// ── Property extraction ─────────────────────────────────────────────────────

ConnmanManagerProps ConnmanManager::extract_manager_props(
    const PropertiesMap& props) {
  ConnmanManagerProps res;
  res.state = get_prop<std::string>(props, "State", "");
  res.offlineMode = get_prop<bool>(props, "OfflineMode", false);
  res.sessionMode = get_prop<bool>(props, "SessionMode", false);
  return res;
}

ConnmanTechnologyProps ConnmanManager::extract_technology_props(
    const std::string& object_path,
    const PropertiesMap& props) {
  ConnmanTechnologyProps res;
  res.objectPath = object_path;
  res.name = get_prop<std::string>(props, "Name", "");
  res.type = get_prop<std::string>(props, "Type", "");
  res.powered = get_prop<bool>(props, "Powered", false);
  res.connected = get_prop<bool>(props, "Connected", false);
  res.tethering = get_prop<bool>(props, "Tethering", false);
  res.tetheringIdentifier =
      get_prop<std::string>(props, "TetheringIdentifier", "");
  res.tetheringPassphrase =
      get_prop<std::string>(props, "TetheringPassphrase", "");
  return res;
}

ConnmanServiceProps ConnmanManager::extract_service_props(
    const std::string& object_path,
    const PropertiesMap& props) {
  ConnmanServiceProps res;
  res.objectPath = object_path;
  res.name = get_prop<std::string>(props, "Name", "");
  res.state = get_prop<std::string>(props, "State", "");
  res.type = get_prop<std::string>(props, "Type", "");

  // "Strength" is represented as a uint8 by connman, safely cast to our int16_t
  auto st_it = props.find("Strength");
  if (st_it != props.end()) {
    try {
      res.strength = static_cast<int16_t>(st_it->second.get<uint8_t>());
    } catch (...) {
      res.strength = 0;
    }
  }

  res.favorite = get_prop<bool>(props, "Favorite", false);
  res.immutable = get_prop<bool>(props, "Immutable", false);
  res.autoConnect = get_prop<bool>(props, "AutoConnect", false);
  res.roaming = get_prop<bool>(props, "Roaming", false);
  res.security = get_prop<std::vector<std::string>>(props, "Security", {});
  res.nameservers =
      get_prop<std::vector<std::string>>(props, "Nameservers", {});
  res.domains = get_prop<std::vector<std::string>>(props, "Domains", {});
  return res;
}

// ── Signal Handlers ─────────────────────────────────────────────────────────

void ConnmanManager::onPropertyChanged(
    [[maybe_unused]] const std::string& name,
    [[maybe_unused]] const sdbus::Variant& value) {
  // Fetch outside the lock — blocking D-Bus call.
  ConnmanManagerProps mgr_props;
  try {
    mgr_props = extract_manager_props(GetProperties());
  } catch (const sdbus::Error& e) {
    std::cerr << "onPropertyChanged fetch failed: " << e.getMessage() << "\n";
    return;
  }

  // Manager props have no object-tree entry; just post under the lock so the
  // post is sequenced with any concurrent get_managed_objects().
  std::scoped_lock lock(obj_tree_mutex_);
  post_glaze(kManagerProps, mgr_props);
}

void ConnmanManager::onTechnologyAdded(
    const sdbus::ObjectPath& path,
    const std::map<std::string, sdbus::Variant>& properties) {
  auto tech_props = extract_technology_props(path, properties);

  std::scoped_lock lock(obj_tree_mutex_);
  technologies_[tech_props.objectPath] = tech_props;
  post_glaze(kTechnologyAdded, tech_props);
}

void ConnmanManager::onTechnologyRemoved(const sdbus::ObjectPath& path) {
  std::scoped_lock lock(obj_tree_mutex_);
  technologies_.erase(path);
  post_glaze(kTechnologyRemoved, ConnmanObjectRemoved{path});
}

void ConnmanManager::onServicesChanged(
    const std::vector<sdbus::Struct<sdbus::ObjectPath,
                                    std::map<std::string, sdbus::Variant>>>&
        changed,
    const std::vector<sdbus::ObjectPath>& removed) {
  // Build updates outside the lock.
  std::vector<ConnmanServiceProps> changed_props;
  changed_props.reserve(changed.size());
  for (const auto& svc : changed) {
    changed_props.push_back(extract_service_props(svc.get<0>(), svc.get<1>()));
  }

  std::scoped_lock lock(obj_tree_mutex_);

  for (const auto& props : changed_props) {
    services_[props.objectPath] = props;
    post_glaze(kServiceChanged, props);
  }

  for (const auto& path : removed) {
    services_.erase(path);
    post_glaze(kServiceRemoved, ConnmanObjectRemoved{path});
  }
}

// ── Dart posting ────────────────────────────────────────────────────────────

template <typename T>
void ConnmanManager::post_glaze(uint8_t discriminator, const T& value) {
  auto payload = glz::encode(value);

  std::vector<uint8_t> buf;
  buf.reserve(1 + payload.size());
  buf.push_back(discriminator);
  buf.insert(buf.end(), payload.begin(), payload.end());

  Dart_CObject obj;
  obj.type = Dart_CObject_kTypedData;
  obj.value.as_typed_data.type = Dart_TypedData_kUint8;
  obj.value.as_typed_data.length = static_cast<intptr_t>(buf.size());
  obj.value.as_typed_data.values = buf.data();
  Dart_PostCObject_DL(events_port_, &obj);
}

// Explicit template instantiations for all posted types.
template void ConnmanManager::post_glaze(uint8_t, const ConnmanManagerProps&);
template void ConnmanManager::post_glaze(uint8_t,
                                         const ConnmanTechnologyProps&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanServiceProps&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanObjectRemoved&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanError&);
