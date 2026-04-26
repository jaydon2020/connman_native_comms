#include "service_bridge.h"

#include <iostream>
#include <map>
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
                            WorkQueue& queue,
                            const std::string& object_path,
                            Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, result_port] {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      ServiceProxy client(*proxy);
      // Increased timeout to 60 seconds for slow RPi connections
      proxy->callMethod("Connect").onInterface("net.connman.Service").withTimeout(60000000);
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
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
