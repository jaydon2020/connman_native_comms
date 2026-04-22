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
  // conn — owned by the sdbus event loop thread (signals, Manager proxy).
  std::unique_ptr<sdbus::IConnection> conn;

  // worker_conn — owned exclusively by WorkQueue's worker thread (one-shot
  // method calls: SetPowered, Scan, Connect, …).  Separate connection so
  // event-loop thread and worker thread never share a connection.
  std::unique_ptr<sdbus::IConnection> worker_conn;

  // Serialises all one-shot method calls on a single background thread.
  // Declared before manager and event_loop_thread so it is destroyed (joined)
  // first in the explicit destructor below.
  std::unique_ptr<WorkQueue> work_queue;

  std::unique_ptr<ConnmanAgent> agent;

  std::unique_ptr<ConnmanManager> manager;
  std::thread event_loop_thread;

  explicit BridgeContext(int64_t events_port) {
    conn = sdbus::createSystemBusConnection();
    worker_conn = sdbus::createSystemBusConnection();
    work_queue = std::make_unique<WorkQueue>();
    manager = std::make_unique<ConnmanManager>(*conn, events_port);

    // Start the event loop BEFORE taking the snapshot (C-2).
    // sdbus-cpp v2 is thread-safe for concurrent method calls, so
    // get_managed_objects() can make blocking D-Bus calls while
    // enterEventLoop() is running on a separate thread.  Starting first
    // ensures that any signals emitted during the snapshot D-Bus calls are
    // already buffered by the running loop and delivered after
    // get_managed_objects() releases obj_tree_mutex_.
    event_loop_thread = std::thread([this]() {
      pthread_setname_np(pthread_self(), "connman_sdbus");
      try {
        conn->enterEventLoop();
      } catch (const std::exception& error) {
        std::cerr << "connman_native_comms: sdbus event loop exception: "
                  << error.what() << "\n";
      }
    });

    // Register the ConnMan Agent
    agent = std::make_unique<ConnmanAgent>(*conn, "/net/connman/native_comms/agent");
    try {
      auto proxy = sdbus::createProxy(*conn, "net.connman", "/");
      proxy->callMethod("RegisterAgent")
           .onInterface("net.connman.Manager")
           .withArguments(agent->get_path());
    } catch (const sdbus::Error& error) {
      std::cerr << "connman_native_comms: Failed to register agent: " << error.what() << "\n";
    }

    // Initial snapshot — fetches all technologies and services, subscribes
    // per-technology PropertyChanged watchers, then posts the snapshot to Dart.
    manager->get_managed_objects();
  }

  ~BridgeContext() noexcept {
    // 0. Unregister the agent
    if (conn && agent) {
      try {
        auto proxy = sdbus::createProxy(*conn, "net.connman", "/");
        proxy->callMethod("UnregisterAgent").onInterface("net.connman.Manager").withArguments(agent->get_path());
      } catch (...) {}
    }
    agent.reset();

    // 1. Drain and join the work queue so no task can outlive worker_conn.
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

    // 4. Close connections last.
    worker_conn.reset();
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
  TechnologyBridge::set_powered(*context->worker_conn, *context->work_queue,
                                object_path, powered, result_port);
}

void connman_technology_scan(void* client,
                             const char* object_path,
                             int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  TechnologyBridge::scan(*context->worker_conn, *context->work_queue,
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
  ServiceBridge::connect(*context->worker_conn, *context->work_queue,
                         object_path, result_port);
}

void connman_service_disconnect(void* client,
                                 const char* object_path,
                                 int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::disconnect(*context->worker_conn, *context->work_queue,
                            object_path, result_port);
}

void connman_service_remove(void* client,
                            const char* object_path,
                            int64_t result_port) {
  if (client == nullptr || object_path == nullptr) {
    return;
  }
  auto* context = static_cast<BridgeContext*>(client);
  ServiceBridge::remove(*context->worker_conn, *context->work_queue,
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
  ServiceBridge::set_auto_connect(*context->worker_conn, *context->work_queue,
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
  ServiceBridge::set_ipv4_config(*context->worker_conn, *context->work_queue,
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
