#include "connman_bridge.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_manager.h"
#include "dart_api_dl.h"
#include "service_bridge.h"
#include "technology_bridge.h"

// ── Bridge Context ──────────────────────────────────────────────────────────

struct BridgeContext {
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<ConnmanManager> manager;
  std::thread event_loop_thread;

  explicit BridgeContext(int64_t events_port) {
    conn = sdbus::createSystemBusConnection();
    manager = std::make_unique<ConnmanManager>(*conn, events_port);

    // Initial snapshot before event loop starts.
    manager->get_managed_objects();

    event_loop_thread = std::thread([this]() {
      pthread_setname_np(pthread_self(), "connman_sdbus");
      try {
        conn->enterEventLoop();
      } catch (const std::exception& e) {
        std::cerr << "connman_native_comms: sdbus event loop exception: "
                  << e.what() << "\n";
      }
    });
  }

  ~BridgeContext() {
    if (conn) {
      conn->leaveEventLoop();
    }
    if (event_loop_thread.joinable()) {
      event_loop_thread.join();
    }
  }
};

// ── C ABI Exports ───────────────────────────────────────────────────────────

intptr_t connman_bridge_init(void* dart_api_data) {
  return Dart_InitializeApiDL(dart_api_data);
}

void* connman_client_create(int64_t events_port) {
  auto* ctx = new BridgeContext(events_port);
  return ctx;
}

void connman_client_destroy(void* client) {
  if (client) {
    delete static_cast<BridgeContext*>(client);
  }
}

// ── Technology Operations ───────────────────────────────────────────────────

void connman_technology_set_powered(void* client,
                                    const char* object_path,
                                    bool powered,
                                    int64_t result_port) {
  if (!client || !object_path)
    return;
  TechnologyBridge::set_powered(object_path, powered, result_port);
}

void connman_technology_scan(void* client,
                             const char* object_path,
                             int64_t result_port) {
  if (!client || !object_path)
    return;
  TechnologyBridge::scan(object_path, result_port);
}

// ── Service Operations ──────────────────────────────────────────────────────

void connman_service_connect(void* client,
                             const char* object_path,
                             int64_t result_port) {
  if (!client || !object_path)
    return;
  ServiceBridge::connect(object_path, result_port);
}

void connman_service_disconnect(void* client,
                                const char* object_path,
                                int64_t result_port) {
  if (!client || !object_path)
    return;
  ServiceBridge::disconnect(object_path, result_port);
}

void connman_service_remove(void* client,
                            const char* object_path,
                            int64_t result_port) {
  if (!client || !object_path)
    return;
  ServiceBridge::remove(object_path, result_port);
}

void connman_service_set_auto_connect(void* client,
                                      const char* object_path,
                                      bool auto_connect,
                                      int64_t result_port) {
  if (!client || !object_path)
    return;
  ServiceBridge::set_auto_connect(object_path, auto_connect, result_port);
}

void connman_service_set_ipv4_config(void* client,
                                     const char* object_path,
                                     const char* method,
                                     const char* address,
                                     const char* netmask,
                                     const char* gateway,
                                     int64_t result_port) {
  if (!client || !object_path || !method || !address || !netmask || !gateway)
    return;
  ServiceBridge::set_ipv4_config(object_path, method, address, netmask, gateway,
                                 result_port);
}
