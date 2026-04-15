#include "connman_manager.h"

#include <iostream>
#include <mutex>

#include "technology_proxy.h"

using namespace connman::msg;

// ── TechWatcher
// ───────────────────────────────────────────────────────────────
//
// Concrete Technology_proxy that forwards PropertyChanged signals to
// ConnmanManager::on_technology_property_changed().  Defined here (not in the
// header) so the full type is available for ConnmanManager's destructor without
// leaking implementation details into the public header.

struct TechWatcherBase {
  std::unique_ptr<sdbus::IProxy> proxy;
  explicit TechWatcherBase(std::unique_ptr<sdbus::IProxy> proxy_in)
      : proxy(std::move(proxy_in)) {}
};

struct TechWatcher : private TechWatcherBase,
                     public net::connman::Technology_proxy {
  ConnmanManager& mgr;
  std::string path;

  TechWatcher(std::unique_ptr<sdbus::IProxy> proxy_ptr,
              ConnmanManager& manager,
              std::string path_in)
      : TechWatcherBase(std::move(proxy_ptr)),
        net::connman::Technology_proxy(*TechWatcherBase::proxy),
        mgr(manager),
        path(std::move(path_in)) {
    registerProxy();
  }

 private:
  void onPropertyChanged(const std::string& name,
                         const sdbus::Variant& value) override {
    mgr.on_technology_property_changed(path, name, value);
  }
};

// ── Lifecycle & Initialization ──────────────────────────────────────────────

ConnmanManager::ConnmanManager(sdbus::IConnection& conn,
                               Dart_Port_DL events_port)
    : ConnmanManagerProxyHolder(conn),
      net::connman::Manager_proxy(*this->proxy_),
      conn_(conn),
      events_port_(events_port) {
  try {
    registerProxy();
  } catch (const sdbus::Error& error) {
    std::cerr << "Manager registerProxy failed: " << error.getMessage() << "\n";
  }
}

// Defined here so TechWatcher is complete at the point of destruction.
ConnmanManager::~ConnmanManager() = default;

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
      auto props = extract_technology_props(tech.get<0>(), tech.get<1>());
      new_technologies[props.objectPath] = props;
    }

    for (const auto& service : GetServices()) {
      auto props = extract_service_props(service.get<0>(), service.get<1>());
      new_services[props.objectPath] = props;
    }
  } catch (const sdbus::Error& error) {
    std::cerr << "ConnmanManager::get_managed_objects failed: "
              << error.getName() << " - " << error.getMessage() << "\n";
    post_glaze(kError, ConnmanError{ConnmanManagerProxyHolder::kService,
                                    error.getName(), error.getMessage()});
    return;
  }

  // Build per-technology watchers outside the lock (I/O operations).
  std::map<std::string, std::unique_ptr<TechWatcher>> new_watchers;
  for (const auto& [path, _] : new_technologies) {
    new_watchers[path] = make_tech_watcher(path);
  }

  // Commit to the object tree and post the initial snapshot atomically so
  // signal handlers cannot interleave their posts with ours.
  //
  // kManagerProps is posted LAST — it acts as the "snapshot complete" sentinel.
  // The Dart ConnmanClient awaits this message before returning from connect();
  // posting it last guarantees kTechnologyProps / kServiceProps are already
  // queued when connect() unblocks.
  std::scoped_lock lock(obj_tree_mutex_);

  cached_mgr_props_ = mgr_props;
  technologies_ = std::move(new_technologies);
  services_ = std::move(new_services);
  tech_watchers_ = std::move(new_watchers);

  for (const auto& [path, tech] : technologies_) {
    post_glaze(kTechnologyProps, tech);
  }
  for (const auto& [path, svc] : services_) {
    post_glaze(kServiceProps, svc);
  }
  post_glaze(kManagerProps, cached_mgr_props_);
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
  auto strength_iterator = props.find("Strength");
  if (strength_iterator != props.end()) {
    try {
      res.strength =
          static_cast<int16_t>(strength_iterator->second.get<uint8_t>());
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
  res.error = get_prop<std::string>(props, "Error", "");
  return res;
}

// ── TechWatcher factory ──────────────────────────────────────────────────────

std::unique_ptr<TechWatcher> ConnmanManager::make_tech_watcher(
    const std::string& path) {
  try {
    auto proxy = sdbus::createProxy(
        conn_, sdbus::ServiceName{ConnmanManagerProxyHolder::kService},
        sdbus::ObjectPath{path});
    return std::make_unique<TechWatcher>(std::move(proxy), *this, path);
  } catch (const sdbus::Error& error) {
    std::cerr << "ConnmanManager: failed to create TechWatcher for " << path
              << ": " << error.getMessage() << "\n";
    return nullptr;
  }
}

// ── Per-technology PropertyChanged ───────────────────────────────────────────

void ConnmanManager::on_technology_property_changed(
    const std::string& path,
    const std::string& name,
    const sdbus::Variant& value) {
  std::scoped_lock lock(obj_tree_mutex_);

  auto tech_iterator = technologies_.find(path);
  if (tech_iterator == technologies_.end()) {
    return;
  }

  auto& tech = tech_iterator->second;

  // Update only the field that changed — avoids a D-Bus round-trip and closes
  // the TOCTOU window of the original full-GetProperties() approach.
  if (name == "Powered") {
    try {
      tech.powered = value.get<bool>();
    } catch (...) {
    }
  } else if (name == "Connected") {
    try {
      tech.connected = value.get<bool>();
    } catch (...) {
    }
  } else if (name == "Tethering") {
    try {
      tech.tethering = value.get<bool>();
    } catch (...) {
    }
  } else if (name == "TetheringIdentifier") {
    try {
      tech.tetheringIdentifier = value.get<std::string>();
    } catch (...) {
    }
  } else if (name == "TetheringPassphrase") {
    try {
      tech.tetheringPassphrase = value.get<std::string>();
    } catch (...) {
    }
  }

  // Reuse kTechnologyAdded — the Dart upsert handler fires technologyChanged
  // for existing entries, which is exactly what we want here.
  post_glaze(kTechnologyAdded, tech);
}

// ── Signal Handlers ─────────────────────────────────────────────────────────

void ConnmanManager::onPropertyChanged(const std::string& name,
                                       const sdbus::Variant& value) {
  // Update the cached snapshot incrementally using the value ConnMan already
  // provided, rather than doing a full GetProperties() round-trip (C-5).
  std::scoped_lock lock(obj_tree_mutex_);

  if (name == "State") {
    try {
      cached_mgr_props_.state = value.get<std::string>();
    } catch (...) {
    }
  } else if (name == "OfflineMode") {
    try {
      cached_mgr_props_.offlineMode = value.get<bool>();
    } catch (...) {
    }
  } else if (name == "SessionMode") {
    try {
      cached_mgr_props_.sessionMode = value.get<bool>();
    } catch (...) {
    }
  }

  post_glaze(kManagerProps, cached_mgr_props_);
}

void ConnmanManager::onTechnologyAdded(
    const sdbus::ObjectPath& path,
    const std::map<std::string, sdbus::Variant>& properties) {
  auto tech_props = extract_technology_props(path, properties);

  // Create the watcher outside the lock (proxy creation is D-Bus I/O).
  auto watcher = make_tech_watcher(path);

  std::scoped_lock lock(obj_tree_mutex_);
  technologies_[tech_props.objectPath] = tech_props;
  if (watcher) {
    tech_watchers_[tech_props.objectPath] = std::move(watcher);
  }
  post_glaze(kTechnologyAdded, tech_props);
}

void ConnmanManager::onTechnologyRemoved(const sdbus::ObjectPath& path) {
  std::scoped_lock lock(obj_tree_mutex_);
  technologies_.erase(path);
  tech_watchers_.erase(path);  // destroy watcher → unregisters signal match
  post_glaze(kTechnologyRemoved, ConnmanObjectRemoved{path});
}

void ConnmanManager::onServicesChanged(
    const std::vector<sdbus::Struct<sdbus::ObjectPath,
                                    std::map<std::string, sdbus::Variant>>>&
        changed,
    const std::vector<sdbus::ObjectPath>& removed) {
  // ConnMan sends PARTIAL property dicts for existing services (only what
  // changed, e.g. just {"State": "association"}).  A full replace via
  // extract_service_props() would zero out Type, Strength, etc., causing those
  // services to vanish from the UI's type-filtered list.  Instead, merge into
  // the existing record and only replace entirely for brand-new services.
  std::scoped_lock lock(obj_tree_mutex_);

  for (const auto& svc : changed) {
    const auto& path    = svc.get<0>();
    const auto& partial = svc.get<1>();

    auto it = services_.find(path);
    if (it == services_.end()) {
      // Brand-new service — extract full props.
      auto props = extract_service_props(path, partial);
      services_[path] = props;
      post_glaze(kServiceChanged, props);
    } else {
      // Existing service — merge only the fields present in the partial dict.
      merge_service_props(it->second, partial);
      post_glaze(kServiceChanged, it->second);
    }
  }

  for (const auto& path : removed) {
    services_.erase(path);
    post_glaze(kServiceRemoved, ConnmanObjectRemoved{path});
  }
}

void ConnmanManager::merge_service_props(ConnmanServiceProps& e,
                                         const PropertiesMap& p) {
  if (p.count("Name"))
    e.name        = get_prop<std::string>(p, "Name",  e.name);
  if (p.count("State"))
    e.state       = get_prop<std::string>(p, "State", e.state);
  if (p.count("Type"))
    e.type        = get_prop<std::string>(p, "Type",  e.type);
  if (p.count("Strength")) {
    try {
      e.strength = static_cast<int16_t>(p.at("Strength").get<uint8_t>());
    } catch (...) {}
  }
  if (p.count("Favorite"))
    e.favorite    = get_prop<bool>(p, "Favorite",    e.favorite);
  if (p.count("Immutable"))
    e.immutable   = get_prop<bool>(p, "Immutable",   e.immutable);
  if (p.count("AutoConnect"))
    e.autoConnect = get_prop<bool>(p, "AutoConnect", e.autoConnect);
  if (p.count("Roaming"))
    e.roaming     = get_prop<bool>(p, "Roaming",     e.roaming);
  if (p.count("Security"))
    e.security    = get_prop<std::vector<std::string>>(p, "Security",
                                                       e.security);
  if (p.count("Nameservers"))
    e.nameservers = get_prop<std::vector<std::string>>(p, "Nameservers",
                                                       e.nameservers);
  if (p.count("Domains"))
    e.domains     = get_prop<std::vector<std::string>>(p, "Domains",
                                                       e.domains);
  if (p.count("Error"))
    e.error       = get_prop<std::string>(p, "Error", e.error);
}

// ── Dart posting ────────────────────────────────────────────────────────────

template <typename T>
void ConnmanManager::post_glaze(uint8_t discriminator, const T& value) {
  auto payload = glz::encode(value);

  std::vector<uint8_t> buffer;
  buffer.reserve(1 + payload.size());
  buffer.push_back(discriminator);
  buffer.insert(buffer.end(), payload.begin(), payload.end());

  Dart_CObject object;
  object.type = Dart_CObject_kTypedData;
  object.value.as_typed_data.type = Dart_TypedData_kUint8;
  object.value.as_typed_data.length = static_cast<intptr_t>(buffer.size());
  object.value.as_typed_data.values = buffer.data();
  Dart_PostCObject_DL(events_port_, &object);
}

// Explicit template instantiations for all posted types.
template void ConnmanManager::post_glaze(uint8_t, const ConnmanManagerProps&);
template void ConnmanManager::post_glaze(uint8_t,
                                         const ConnmanTechnologyProps&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanServiceProps&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanObjectRemoved&);
template void ConnmanManager::post_glaze(uint8_t, const ConnmanError&);
