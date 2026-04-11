#include "connman_manager.h"

#include <iostream>

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
  try {
    // 1. Fetch Manager Properties
    auto props = GetProperties();
    auto mgr_props = extract_manager_props(props);
    post_glaze(kManagerProps, mgr_props);

    // 2. Fetch all Technologies
    auto techs = GetTechnologies();
    for (const auto& tech : techs) {
      auto tech_props = extract_technology_props(tech.get<0>(), tech.get<1>());
      post_glaze(kTechnologyProps, tech_props);
    }

    // 3. Fetch all Services
    auto services = GetServices();
    for (const auto& svc : services) {
      auto svc_props = extract_service_props(svc.get<0>(), svc.get<1>());
      post_glaze(kServiceProps, svc_props);
    }

  } catch (const sdbus::Error& e) {
    std::cerr << "ConnmanManager::get_managed_objects failed: " << e.getName()
              << " - " << e.getMessage() << "\n";
    ConnmanError err{ConnmanManagerProxyHolder::kConnmanService, e.getName(), e.getMessage()};
    post_glaze(kError, err);
  }
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

void ConnmanManager::onPropertyChanged([[maybe_unused]] const std::string& name,
                                       [[maybe_unused]] const sdbus::Variant& value) {
  try {
    // Fetch the full properties map on every change rather than applying partial
    // updates — keeps the Dart side stateless and avoids partial-update handlers.
    auto props = GetProperties();
    auto mgr_props = extract_manager_props(props);
    post_glaze(kManagerProps, mgr_props);
  } catch (const sdbus::Error& e) {
    std::cerr << "onPropertyChanged fetch failed: " << e.getMessage() << "\n";
  }
}

void ConnmanManager::onTechnologyAdded(
    const sdbus::ObjectPath& path,
    const std::map<std::string, sdbus::Variant>& properties) {
  auto tech_props = extract_technology_props(path, properties);
  post_glaze(kTechnologyAdded, tech_props);
}

void ConnmanManager::onTechnologyRemoved(const sdbus::ObjectPath& path) {
  ConnmanObjectRemoved removed{path};
  post_glaze(kTechnologyRemoved, removed);
}

void ConnmanManager::onServicesChanged(
    const std::vector<sdbus::Struct<sdbus::ObjectPath,
                                    std::map<std::string, sdbus::Variant>>>& changed,
    const std::vector<sdbus::ObjectPath>& removed) {
  // Process all modified or added services
  for (const auto& svc : changed) {
    auto svc_props = extract_service_props(svc.get<0>(), svc.get<1>());
    post_glaze(kServiceChanged, svc_props);
  }

  // Process all removed services
  for (const auto& rm_path : removed) {
    ConnmanObjectRemoved obj{rm_path};
    post_glaze(kServiceRemoved, obj);
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
template void ConnmanManager::post_glaze(uint8_t, const ConnmanTechnologyProps&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanServiceProps&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanObjectRemoved&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanError&);
