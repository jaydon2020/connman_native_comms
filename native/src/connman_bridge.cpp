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

struct BridgeContext {
  std::unique_ptr<sdbus::IConnection> conn;
  std::unique_ptr<WorkQueue> work_queue;
  std::unique_ptr<ConnmanAgent> agent;
  std::unique_ptr<ConnmanManager> manager;
  std::thread event_loop_thread;

  explicit BridgeContext(int64_t events_port) {
    // Request a stable name so ConnMan can reliably call back to our process.
    std::string bus_name = "net.connman.native_comms.agent" + std::to_string(getpid());
    conn = sdbus::createSystemBusConnection(sdbus::ServiceName(bus_name));
    work_queue = std::make_unique<WorkQueue>();
    manager = std::make_unique<ConnmanManager>(*conn, events_port);

    event_loop_thread = std::thread([this]() {
      pthread_setname_np(pthread_self(), "connman_sdbus");
      try { conn->enterEventLoop(); } catch (...) {}
    });

    try {
      std::string agent_path = "/net/connman/native_comms/agent";
      agent = std::make_unique<ConnmanAgent>(*conn, sdbus::ObjectPath{agent_path}, events_port);
      auto proxy = sdbus::createProxy(*conn, sdbus::ServiceName{"net.connman"}, sdbus::ObjectPath{"/"});
      
      std::cerr << "connman_native_comms: [Bridge] Registering agent...\n";
      proxy->callMethod("RegisterAgent").onInterface("net.connman.Manager").withArguments(agent->get_path()).withTimeout(5000000);
      std::cerr << "connman_native_comms: [Bridge] Agent registered.\n";
    } catch (const std::exception& e) {
      std::cerr << "connman_native_comms: [Bridge] Agent error: " << e.what() << "\n";
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

void* connman_client_create(void* data, int64_t port) {
  if (Dart_InitializeApiDL(data) != 0) return nullptr;
  try { return new BridgeContext(port); } catch (...) { return nullptr; }
}
void connman_client_destroy(void* c) { if (c) delete static_cast<BridgeContext*>(c); }
void connman_technology_set_powered(void* c, const char* p, bool pow, int64_t r) { TechnologyBridge::set_powered(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, pow, r); }
void connman_technology_scan(void* c, const char* p, int64_t r) { TechnologyBridge::scan(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, r); }
void connman_service_connect(void* c, const char* p, int64_t r) { ServiceBridge::connect(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, r); }
void connman_service_disconnect(void* c, const char* p, int64_t r) { ServiceBridge::disconnect(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, r); }
void connman_service_remove(void* c, const char* p, int64_t r) { ServiceBridge::remove(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, r); }
void connman_service_set_auto_connect(void* c, const char* p, bool a, int64_t r) { ServiceBridge::set_auto_connect(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, a, r); }
void connman_service_set_ipv4_config(void* c, const char* p, const char* m, const char* a, const char* n, const char* g, int64_t r) { ServiceBridge::set_ipv4_config(*static_cast<BridgeContext*>(c)->conn, *static_cast<BridgeContext*>(c)->work_queue, p, m, a, n, g, r); }
void connman_agent_set_passphrase(void* c, const char* p, const char* pass) { static_cast<BridgeContext*>(c)->agent->set_passphrase(p, pass); }
void connman_agent_clear_passphrase(void* c, const char* p) { static_cast<BridgeContext*>(c)->agent->clear_passphrase(p); }
