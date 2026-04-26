#include "connman_bridge.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_agent.h"
#include "connman_manager.h"
#include "dart_api_dl.h"
#include "service_bridge.h"
#include "technology_bridge.h"
#include "work_queue.h"

// ── Bridge Context ──────────────────────────────────────────────────────────

struct BridgeContext {
  // conn — shared by the sdbus event loop thread (signals, Manager proxy)
  // and the worker thread (one-shot method calls: SetPowered, Scan, Connect, …).
  // sdbus-cpp v2 is thread-safe for concurrent method calls on a shared connection.
  std::unique_ptr<sdbus::IConnection> conn;

  // Serialises all one-shot method calls on a single background thread.
  // Declared before manager and event_loop_thread so it is destroyed (joined)
  // first in the explicit destructor below.
  std::unique_ptr<WorkQueue> work_queue;

  std::unique_ptr<ConnmanAgent> agent;

  std::unique_ptr<ConnmanManager> manager;
  std::thread event_loop_thread;

  explicit BridgeContext(int64_t events_port) {
    conn = sdbus::createSystemBusConnection();
    work_queue = std::make_unique<WorkQueue>();
    manager = std::make_unique<ConnmanManager>(*conn, events_port);

    // Start the event loop BEFORE taking the snapshot (C-2).
    event_loop_thread = std::thread([this]() {
      pthread_setname_np(pthread_self(), "connman_sdbus");
      try {
        conn->enterEventLoop();
      } catch (const std::exception& error) {
        std::cerr << "connman_native_comms: sdbus event loop exception: "
                  << error.what() << "\n";
      }
    });

    try {
      // Register the ConnMan Agent
      agent = std::make_unique<ConnmanAgent>(
          *conn, sdbus::ObjectPath{"/net/connman/native_comms/agent"},
          events_port);
      auto proxy = sdbus::createProxy(
          *conn, sdbus::ServiceName{"net.connman"}, sdbus::ObjectPath{"/"});
      proxy->callMethod("RegisterAgent")
          .onInterface("net.connman.Manager")
          .withArguments(agent->get_path());
    } catch (const sdbus::Error& error) {
      std::cerr << "connman_native_comms: Failed to register agent: "
                << error.what() << "\n";
      throw; // Fail bridge creation if ConnMan is not available
    } catch (const std::exception& error) {
      std::cerr << "connman_native_comms: Failed during initialization: "
                << error.what() << "\n";
      conn->leaveEventLoop();
      if (event_loop_thread.joinable()) {
        event_loop_thread.join();
      }
      throw;
    }

    // Initial snapshot — fetches all technologies and services, subscribes
    // per-technology PropertyChanged watchers, then posts the snapshot to Dart.
    manager->get_managed_objects();
  }

  ~BridgeContext() noexcept {
    // 0. Unregister the agent
    if (conn && agent) {
      try {
        auto proxy =
            sdbus::createProxy(*conn, sdbus::ServiceName{"net.connman"},
                               sdbus::ObjectPath{"/"});
        proxy->callMethod("UnregisterAgent")
            .onInterface("net.connman.Manager")
            .withArguments(agent->get_path())
            .dontExpectReply();
      } catch (...) {}
    }
    agent.reset();

    // 1. Drain and join the work queue.
    work_queue.reset();

    // 2. Signal the event loop to exit and wait for it to finish.
    if (conn) {
      conn->leaveEventLoop();
    }
    if (event_loop_thread.joinable()) {
      event_loop_thread.join();
    }

    // 3. Destroy manager (removes signal handlers and TechWatchers).
    manager.reset();

    // 4. Close connection last.
    conn.reset();
  }
};

// ── C++ ABI Exports ─────────────────────────────────────────────────────────

void* connman_client_create(void* dart_api_data, int64_t events_port) {
  if (Dart_InitializeApiDL(dart_api_data) != 0) {
    std::cerr << "connman_native_comms: Dart_InitializeApiDL failed\n";
    return nullptr;
  }
  try {
    return new BridgeContext(events_port);
  } catch (const std::exception& error) {
    std::cerr << "connman_native_comms: BridgeContext creation failed: "
              << error.what() << "\n";
    return nullptr;
  }
}

void connman_client_destroy(void* client) {
  if (client != nullptr) {
    delete static_cast<BridgeContext*>(client);
  }
}

// ── Technology Operations ───────────────────────────────────────────────────

void connman_technology_set_powered(void* client,
                                    const char* object_path,
                                    bool powered,
                                    int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  TechnologyBridge::set_powered(*context->conn, *context->work_queue,
                                object_path, powered, result_port);
}

void connman_technology_scan(void* client,
                             const char* object_path,
                             int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  TechnologyBridge::scan(*context->conn, *context->work_queue,
                         object_path, result_port);
}

// ── Service Operations ──────────────────────────────────────────────────────

void connman_service_connect(void* client,
                             const char* object_path,
                             int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::connect(*context->conn, *context->work_queue,
                         object_path, result_port);
}

void connman_service_disconnect(void* client,
                                const char* object_path,
                                int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::disconnect(*context->conn, *context->work_queue,
                            object_path, result_port);
}

void connman_service_remove(void* client,
                            const char* object_path,
                            int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::remove(*context->conn, *context->work_queue,
                        object_path, result_port);
}

void connman_service_set_auto_connect(void* client,
                                      const char* object_path,
                                      bool auto_connect,
                                      int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::set_auto_connect(*context->conn, *context->work_queue,
                                  object_path, auto_connect, result_port);
}

void connman_service_set_ipv4_config(void* client,
                                     const char* object_path,
                                     const char* method,
                                     const char* address,
                                     const char* netmask,
                                     const char* gateway,
                                     int64_t result_port) {
  if (client == nullptr || object_path == nullptr || method == nullptr ||
      address == nullptr || netmask == nullptr || gateway == nullptr) {
    return;
  }

  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::set_ipv4_config(*context->conn, *context->work_queue,
                                 object_path, method, address, netmask, gateway,
                                 result_port);
}

// ── Agent Operations ────────────────────────────────────────────────────────

void connman_agent_set_passphrase(void* client,
                                  const char* object_path,
                                  const char* passphrase) {
  if (client == nullptr || object_path == nullptr || passphrase == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  if (context->agent) {
    context->agent->set_passphrase(object_path, passphrase);
  }
}

void connman_agent_clear_passphrase(void* client, const char* object_path) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  if (context->agent) {
    context->agent->clear_passphrase(object_path);
  }
}
