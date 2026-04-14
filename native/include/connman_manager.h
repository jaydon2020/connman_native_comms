// connman_manager.h — ConnMan Manager proxy + state management.
//
// Manages the ConnMan D-Bus object tree by:
//   1. Pulling GetProperties(), GetTechnologies(), GetServices() as snapshot.
//   2. Subscribing to PropertyChanged, TechnologyAdded/Removed, ServicesChanged
//   3. Subscribing to net.connman.Technology.PropertyChanged on each technology
//      so per-technology power/connected state changes are delivered live
//      (S-1).
//   4. Posting all changes to Dart via Dart_PostCObject_DL
//
// All sdbus-cpp signal callbacks run on the sdbus event loop thread.
// The object tree maps are protected by obj_tree_mutex_.

#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "connman_types.h"
#include "dart_api_dl.h"
#include "manager_proxy.h"

// Forward-declared so that the unique_ptr<TechWatcher> member compiles without
// requiring TechWatcher's full definition in this header.  The destructor is
// defined in connman_manager.cpp after TechWatcher is complete.
struct TechWatcher;

// Base struct to guarantee sdbus::IProxy initializes before Manager_proxy.
struct ConnmanManagerProxyHolder {
  static constexpr auto kService = "net.connman";
  static constexpr auto kManagerPath = "/";

  std::unique_ptr<sdbus::IProxy> proxy_;
  explicit ConnmanManagerProxyHolder(sdbus::IConnection& conn)
      : proxy_(sdbus::createProxy(conn,
                                  sdbus::ServiceName{kService},
                                  sdbus::ObjectPath{kManagerPath})) {}
};

class ConnmanManager : private ConnmanManagerProxyHolder,
                       public net::connman::Manager_proxy {
 public:
  using PropertiesMap = std::map<std::string, sdbus::Variant>;

  ConnmanManager(sdbus::IConnection& conn, Dart_Port_DL events_port);

  // Defined in .cpp so TechWatcher is complete at the point of destruction.
  ~ConnmanManager();

  ConnmanManager(const ConnmanManager&) = delete;
  ConnmanManager& operator=(const ConnmanManager&) = delete;

  // Snapshot the current ConnMan state via GetProperties(), GetTechnologies(),
  // GetServices(). Posts initial state down to the events_port and subscribes
  // to per-technology PropertyChanged signals.
  void get_managed_objects();

  // ── Property extraction from D-Bus Variant maps ─────────────────────────

  static ConnmanManagerProps extract_manager_props(const PropertiesMap& props);

  static ConnmanTechnologyProps extract_technology_props(
      const std::string& object_path,
      const PropertiesMap& props);

  static ConnmanServiceProps extract_service_props(
      const std::string& object_path,
      const PropertiesMap& props);

  // Safe property accessors (return default on missing/type-mismatch).
  template <typename T>
  static T get_prop(const PropertiesMap& props,
                    const std::string& key,
                    const T& fallback = {});

  // Called by TechWatcher when a per-technology PropertyChanged signal fires.
  void on_technology_property_changed(const std::string& path,
                                      const std::string& name,
                                      const sdbus::Variant& value);

 protected:
  // ── Signal handlers from Manager_proxy ──────────────────────────────────
  void onPropertyChanged(const std::string& name,
                         const sdbus::Variant& value) override;

  void onTechnologyAdded(
      const sdbus::ObjectPath& path,
      const std::map<std::string, sdbus::Variant>& properties) override;

  void onTechnologyRemoved(const sdbus::ObjectPath& path) override;

  void onServicesChanged(
      const std::vector<sdbus::Struct<sdbus::ObjectPath,
                                      std::map<std::string, sdbus::Variant>>>&
          changed,
      const std::vector<sdbus::ObjectPath>& removed) override;

  void onPeersChanged(
      const std::vector<sdbus::Struct<sdbus::ObjectPath,
                                      std::map<std::string, sdbus::Variant>>>&
          changed,
      const std::vector<sdbus::ObjectPath>& removed) override {}

  void onTetheringClientsChanged(
      const std::vector<std::string>& registered,
      const std::vector<std::string>& removed) override {}

 private:
  sdbus::IConnection& conn_;
  Dart_Port_DL events_port_;

  // Cached manager-level properties — updated incrementally in
  // onPropertyChanged() rather than via a full GetProperties() round-trip.
  ConnmanManagerProps cached_mgr_props_;

  // ── Object tree ─────────────────────────────────────────────────────────
  // Mirrors the live ConnMan object tree in memory.
  // Written by get_managed_objects() (caller thread) and by signal handlers
  // (sdbus event loop thread) — both must hold obj_tree_mutex_.

  mutable std::mutex obj_tree_mutex_;
  std::map<std::string, ConnmanTechnologyProps> technologies_;
  std::map<std::string, ConnmanServiceProps> services_;

  // Per-technology signal watchers (one per technology object path).
  // Created in get_managed_objects() and onTechnologyAdded(), removed in
  // onTechnologyRemoved().  Protected by obj_tree_mutex_.
  std::map<std::string, std::unique_ptr<TechWatcher>> tech_watchers_;

  // Creates a TechWatcher for the given technology object path without taking
  // the mutex (proxy creation involves D-Bus I/O).
  std::unique_ptr<TechWatcher> make_tech_watcher(const std::string& path);

  // ── Dart posting helper ─────────────────────────────────────────────────

  template <typename T>
  void post_glaze(uint8_t discriminator, const T& value);
};

// ── Template implementations ──────────────────────────────────────────────

template <typename T>
inline T ConnmanManager::get_prop(const PropertiesMap& props,
                                  const std::string& key,
                                  const T& fallback) {
  auto it = props.find(key);
  if (it == props.end()) {
    return fallback;
  }
  // Catch std::bad_cast / std::invalid_argument from sdbus Variant, plus any
  // sdbus::Error — the original catch only covered the latter.
  try {
    return it->second.get<T>();
  } catch (const std::exception&) {
    return fallback;
  } catch (...) {
    return fallback;
  }
}
