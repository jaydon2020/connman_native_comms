#include "connman_bridge.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_agent.h"
#include "connman_manager.h"
#include "dart_api_dl.h"
#include "service_bridge.h"
#include "technology_bridge.h"
#include "work_queue.h"

// ── Bridge Context ──────────────────────────────────────────────────────────

struct BridgeContext {
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<WorkQueue> work_queue;
  std::unique_ptr<ConnmanAgent> agent;
  std::unique_ptr<ConnmanManager> manager;
  std::thread event_loop_thread;

  explicit BridgeContext(int64_t events_port) {
    // Request a unique bus name to ensure system bus permissions for the Agent
    std::string bus_name = "net.connman.native_comms.client" + std::to_string(getpid());
    conn = sdbus::createSystemBusConnection(sdbus::ServiceName(bus_name));
    
    work_queue = std::make_unique<WorkQueue>();
    manager = std::make_unique<ConnmanManager>(*conn, events_port);

    event_loop_thread = std::thread([this]() {
      pthread_setname_np(pthread_self(), "connman_sdbus");
      try {
        conn->enterEventLoop();
      } catch (const std::exception& error) {
        std::cerr << "connman_native_comms: sdbus loop error: " << error.what() << "\n";
      }
    });

    try {
      std::string agent_path = "/net/connman/native_comms/agent";
      agent = std::make_unique<ConnmanAgent>(*conn, sdbus::ObjectPath{agent_path}, events_port);
      
      auto proxy = sdbus::createProxy(*conn, sdbus::ServiceName{"net.connman"}, sdbus::ObjectPath{"/"});
      
      std::cerr << "connman_native_comms: Agent setup at " << agent_path << "\n";
      
      // Force unregister any existing agent at this path before registering ours
      try {
        proxy->callMethod("UnregisterAgent").onInterface("net.connman.Manager").withArguments(agent->get_path()).withTimeout(5000000);
      } catch (...) {}

      proxy->callMethod("RegisterAgent")
          .onInterface("net.connman.Manager")
          .withArguments(agent->get_path())
          .withTimeout(5000000);
          
      std::cerr << "connman_native_comms: Agent registration successful\n";
    } catch (const std::exception& error) {
      std::cerr << "connman_native_comms: Agent setup warning: " << error.what() << "\n";
    }

    manager->get_managed_objects();
  }

  ~BridgeContext() noexcept {
    if (conn && agent) {
      try {
        auto proxy = sdbus::createProxy(*conn, sdbus::ServiceName{"net.connman"}, sdbus::ObjectPath{"/"});
        proxy->callMethod("UnregisterAgent").onInterface("net.connman.Manager").withArguments(agent->get_path()).dontExpectReply();
      } catch (...) {}
    }
    agent.reset();
    work_queue.reset();
    if (conn) conn->leaveEventLoop();
    if (event_loop_thread.joinable()) event_loop_thread.join();
    manager.reset();
    conn.reset();
  }
};

void* connman_client_create(void* dart_api_data, int64_t events_port) {
  if (Dart_InitializeApiDL(dart_api_data) != 0) return nullptr;
  try {
    return new BridgeContext(events_port);
  } catch (const std::exception& error) {
    std::cerr << "connman_native_comms: Create failed: " << error.what() << "\n";
    return nullptr;
  }
}

void connman_client_destroy(void* client) {
  if (client) delete static_cast<BridgeContext*>(client);
}

// ── Shared Method Dispatch Helpers ──────────────────────────────────────────

void connman_technology_set_powered(void* client, const char* path, bool p, int64_t port) {
  if (client && path) TechnologyBridge::set_powered(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, p, port);
}

void connman_technology_scan(void* client, const char* path, int64_t port) {
  if (client && path) TechnologyBridge::scan(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, port);
}

void connman_service_connect(void* client, const char* path, int64_t port) {
  if (client && path) ServiceBridge::connect(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, port);
}

void connman_service_disconnect(void* client, const char* path, int64_t port) {
  if (client && path) ServiceBridge::disconnect(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, port);
}

void connman_service_remove(void* client, const char* path, int64_t port) {
  if (client && path) ServiceBridge::remove(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, port);
}

void connman_service_set_auto_connect(void* client, const char* path, bool a, int64_t port) {
  if (client && path) ServiceBridge::set_auto_connect(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, a, port);
}

void connman_service_set_ipv4_config(void* client, const char* path, const char* m, const char* a, const char* n, const char* g, int64_t port) {
  if (client && path && m && a && n && g) ServiceBridge::set_ipv4_config(*static_cast<BridgeContext*>(client)->conn, *static_cast<BridgeContext*>(client)->work_queue, path, m, a, n, g, port);
}

void connman_agent_set_passphrase(void* client, const char* path, const char* p) {
  if (client && path && p && static_cast<BridgeContext*>(client)->agent) static_cast<BridgeContext*>(client)->agent->set_passphrase(path, p);
}

void connman_agent_clear_passphrase(void* client, const char* path) {
  if (client && path && static_cast<BridgeContext*>(client)->agent) static_cast<BridgeContext*>(client)->agent->clear_passphrase(path);
}
