#include "technology_bridge.h"

#include <iostream>
#include <string>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_types.h"
#include "technology_proxy.h"

namespace {

constexpr auto kConnmanService = "net.connman";

// Minimal concrete proxy — signals are unused for one-shot operations.
struct TechnologyProxy : public net::connman::Technology_proxy {
  explicit TechnologyProxy(sdbus::IProxy& proxy_in)
      : net::connman::Technology_proxy(proxy_in) {}
  void onPropertyChanged(const std::string& name,
                         const sdbus::Variant& value) override {
    (void)name;
    (void)value;
  }
  // Note: registerProxy() is intentionally NOT called — signals are not needed
  // for one-shot set/scan operations.
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

void TechnologyBridge::set_powered(sdbus::IConnection& conn,
                                   WorkQueue& queue,
                                   const std::string& object_path,
                                   bool powered,
                                   Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, powered, result_port] {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      TechnologyProxy client(*proxy);
      client.SetProperty("Powered", sdbus::Variant(powered));
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
}

void TechnologyBridge::scan(sdbus::IConnection& conn,
                            WorkQueue& queue,
                            const std::string& object_path,
                            Dart_Port_DL result_port) {
  queue.enqueue([&conn, object_path, result_port] {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName(kConnmanService),
                                      sdbus::ObjectPath(object_path));
      TechnologyProxy client(*proxy);
      client.Scan();
      post_success(result_port, object_path);
    } catch (const sdbus::Error& error) {
      post_error(result_port, object_path, error.getName(), error.getMessage());
    }
  });
}
