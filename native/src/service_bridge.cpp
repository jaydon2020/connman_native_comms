#include "service_bridge.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_types.h"
#include "service_proxy.h"

namespace {

static constexpr auto kConnmanService = "net.connman";

// Minimal concrete proxy — signals are unused for one-shot operations.
struct ServiceProxy : public net::connman::Service_proxy {
  explicit ServiceProxy(sdbus::IProxy& p) : net::connman::Service_proxy(p) {}
  void onPropertyChanged(const std::string&, const sdbus::Variant&) override {}
  // Note: registerProxy() is intentionally NOT called — signals are not needed
  // for one-shot connect/disconnect/property operations.
};

// ── Dart posting
// ──────────────────────────────────────────────────────────────

template <typename T>
void post_glaze(Dart_Port_DL port, uint8_t discriminator, const T& value) {
  auto payload = glz::encode(value);

  std::vector<uint8_t> buf;
  buf.reserve(1 + payload.size());
  buf.push_back(discriminator);
  buf.insert(buf.end(), payload.begin(), payload.end());

  Dart_CObject obj;
  obj.type = Dart_CObject_kTypedData;
  obj.value.as_typed_data.type = Dart_TypedData_kUint8;
  obj.value.as_typed_data.length = static_cast<intptr_t>(buf.size());
  obj.value.as_typed_data.values = buf.data();
  Dart_PostCObject_DL(port, &obj);
}

void post_success(Dart_Port_DL port, const std::string& object_path) {
  post_glaze(port, connman::msg::kDone, ConnmanMethodSuccess{object_path});
}

void post_error(Dart_Port_DL port,
                const std::string& object_path,
                const sdbus::Error& e) {
  post_glaze(port, connman::msg::kError,
             ConnmanError{object_path, e.getName(), e.getMessage()});
}

// ── Shared-connection dispatch
// ────────────────────────────────────────────────

template <typename Func>
void dispatch(sdbus::IConnection& conn,
              WorkQueue& queue,
              std::string object_path,
              Dart_Port_DL result_port,
              Func&& func) {
  queue.enqueue([&conn, object_path = std::move(object_path), result_port,
                 func = std::forward<Func>(func)]() mutable {
    try {
      auto proxy = sdbus::createProxy(conn, sdbus::ServiceName{kConnmanService},
                                      sdbus::ObjectPath{object_path});
      ServiceProxy svc(*proxy);
      func(svc);
      post_success(result_port, object_path);
    } catch (const sdbus::Error& e) {
      std::cerr << "ServiceBridge (" << object_path << "): " << e.getName()
                << " — " << e.getMessage() << "\n";
      post_error(result_port, object_path, e);
    }
  });
}

}  // namespace

void ServiceBridge::connect(sdbus::IConnection& conn,
                            WorkQueue& queue,
                            const std::string& object_path,
                            Dart_Port_DL result_port) {
  dispatch(conn, queue, object_path, result_port,
           [](auto& svc) { svc.Connect(); });
}

void ServiceBridge::disconnect(sdbus::IConnection& conn,
                               WorkQueue& queue,
                               const std::string& object_path,
                               Dart_Port_DL result_port) {
  dispatch(conn, queue, object_path, result_port,
           [](auto& svc) { svc.Disconnect(); });
}

void ServiceBridge::remove(sdbus::IConnection& conn,
                           WorkQueue& queue,
                           const std::string& object_path,
                           Dart_Port_DL result_port) {
  dispatch(conn, queue, object_path, result_port,
           [](auto& svc) { svc.Remove(); });
}

void ServiceBridge::set_auto_connect(sdbus::IConnection& conn,
                                     WorkQueue& queue,
                                     const std::string& object_path,
                                     bool auto_connect,
                                     Dart_Port_DL result_port) {
  dispatch(conn, queue, object_path, result_port, [auto_connect](auto& svc) {
    svc.SetProperty("AutoConnect", sdbus::Variant{auto_connect});
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
  dispatch(conn, queue, object_path, result_port,
           [method, address, netmask, gateway](auto& svc) {
             std::map<std::string, sdbus::Variant> config;
             config["Method"] = sdbus::Variant{method};
             if (method == "manual") {
               config["Address"] = sdbus::Variant{address};
               config["Netmask"] = sdbus::Variant{netmask};
               config["Gateway"] = sdbus::Variant{gateway};
             }
             svc.SetProperty("IPv4.Configuration", sdbus::Variant{config});
           });
}
