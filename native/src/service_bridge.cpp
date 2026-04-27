#include "service_bridge.h"

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_types.h"
#include "service_proxy.h"

namespace {

constexpr auto kConnmanService = "net.connman";

// Minimal concrete proxy — signals are unused for one-shot operations.
struct ServiceProxy : public net::connman::Service_proxy {
  explicit ServiceProxy(sdbus::IProxy& proxy_in)
      : net::connman::Service_proxy(proxy_in) {}
  void onPropertyChanged(const std::string& name,
                         const sdbus::Variant& value) override {
    (void)name;
    (void)value;
  }
  // Note: registerProxy() is intentionally NOT called — signals are not needed
  // for one-shot connect/disconnect/property operations.
};

// ── Dart posting
// ──────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(bugprone-argument-selection)
template <typename T>
void post_glaze(Dart_Port_DL dart_port,
                uint8_t message_discriminator,
                const T& value) {
  auto payload = glz::encode(value);

  std::vector<uint8_t> buffer;
  buffer.reserve(1 + payload.size());
  buffer.push_back(message_discriminator);
  buffer.insert(buffer.end(), payload.begin(), payload.end());

  Dart_CObject object;
  object.type = Dart_CObject_kTypedData;
  object.value.as_typed_data.type = Dart_TypedData_kUint8;
  object.value.as_typed_data.length = static_cast<intptr_t>(buffer.size());
  object.value.as_typed_data.values = buffer.data();
  Dart_PostCObject_DL(dart_port, &object);
}

void post_success(Dart_Port_DL port, const std::string& object_path) {
  post_glaze(port, connman::msg::kDone, ConnmanMethodSuccess{object_path});
}

void post_error(Dart_Port_DL port,
                const std::string& object_path,
                const std::string& error_name,
                const std::string& error_message) {
  post_glaze(port, connman::msg::kError,
             ConnmanError{object_path, error_name, error_message});
}

}  // namespace

void ServiceBridge::connect(sdbus::IConnection& conn,
                            WorkQueue& /*queue*/,
                            const std::string& object_path,
                            Dart_Port_DL result_port) {
  // IMPORTANT: Service.Connect() is dispatched as an ASYNC call on the event
  // loop's connection, NOT as a synchronous call on the WorkQueue thread.
  //
  // Why: When ConnMan needs credentials (PSK, WPA-Enterprise, etc.) it calls
  // back to our Agent.RequestInput() method.  That callback must be dispatched
  // by the sdbus event loop thread.  If we held a synchronous sd_bus_call() on
  // the WorkQueue thread, the underlying bus mutex would be held during the
  // wait, preventing the event loop from dispatching the Agent callback.
  // This would deadlock: Connect() waits for the Agent → Agent can't run
  // because the bus is locked by Connect().
  //
  // Using callMethodAsync() avoids the mutex contention: the outgoing message
  // is sent and the event loop thread is free to process the Agent callback
  // when it arrives.  The async reply handler then posts kDone/kError to Dart.

  try {
    auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                    sdbus::ObjectPath(object_path));

    // Move proxy into shared_ptr so it stays alive until the async callback fires.
    auto shared_proxy = std::shared_ptr<sdbus::IProxy>(std::move(proxy));

    shared_proxy->callMethodAsync("Connect")
        .onInterface("net.connman.Service")
        .withTimeout(60000000)
        .uponReplyInvoke([shared_proxy, object_path, result_port](
                              std::optional<sdbus::Error> error) {
          if (error) {
            post_error(result_port, object_path, error->getName(),
                       error->getMessage());
          } else {
            post_success(result_port, object_path);
          }
          // shared_proxy ref dropped here — destructor fires after callback.
        });
  } catch (const sdbus::Error& error) {
    post_error(result_port, object_path, error.getName(), error.getMessage());
  }
}

void ServiceBridge::disconnect(sdbus::IConnection& conn,
                               WorkQueue& queue,
                               const std::string& object_path,
                               Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, result_port] {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      ServiceProxy client(*proxy);
      // 60s timeout
      proxy->callMethod("Disconnect").onInterface("net.connman.Service").withTimeout(60000000);
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
}

void ServiceBridge::remove(sdbus::IConnection& conn,
                            WorkQueue& queue,
                            const std::string& object_path,
                            Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, result_port] {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      ServiceProxy client(*proxy);
      // 60s timeout
      proxy->callMethod("Remove").onInterface("net.connman.Service").withTimeout(60000000);
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
}
void ServiceBridge::set_auto_connect(sdbus::IConnection& conn,
                                     WorkQueue& queue,
                                     const std::string& object_path,
                                     bool auto_connect,
                                     Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, auto_connect, result_port] {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      ServiceProxy client(*proxy);
      client.SetProperty("AutoConnect", sdbus::Variant(auto_connect));
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
}

void ServiceBridge::set_ipv4_config(sdbus::IConnection& conn,
                                    WorkQueue& queue,
                                    const std::string& object_path,
                                    const std::string& method,
                                    const std::string& address,
                                    const std::string& netmask,
                                    const std::string& gateway,
                                    Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, method, address, netmask, gateway,
                 result_port] {
    try {
      std::map<std::string, sdbus::Variant> config;
      config["Method"] = sdbus::Variant(method);
      config["Address"] = sdbus::Variant(address);
      config["Netmask"] = sdbus::Variant(netmask);
      config["Gateway"] = sdbus::Variant(gateway);

      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      ServiceProxy client(*proxy);
      client.SetProperty("IPv4.Configuration", sdbus::Variant(config));
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
}
