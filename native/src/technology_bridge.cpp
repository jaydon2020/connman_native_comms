#include "technology_bridge.h"

#include <iostream>
#include <string>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>

#include "connman_types.h"
#include "technology_proxy.h"

namespace {

static constexpr auto kConnmanService = "net.connman";

// Minimal concrete proxy — signals are unused for one-shot operations.
struct TechnologyProxy : public net::connman::Technology_proxy {
  explicit TechnologyProxy(sdbus::IProxy& p)
      : net::connman::Technology_proxy(p) {}
  void onPropertyChanged(const std::string&, const sdbus::Variant&) override {}
  // Note: registerProxy() is intentionally NOT called — signals are not needed
  // for one-shot set/scan operations.
};

// ── Dart posting ──────────────────────────────────────────────────────────────

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

// ── Shared-connection dispatch ────────────────────────────────────────────────
//
// Enqueues a task on the WorkQueue.  The task creates a short-lived proxy on
// the shared worker connection (conn), calls the D-Bus method, and posts the
// result.  conn is captured by reference — it is guaranteed to outlive all
// queued tasks because WorkQueue::~WorkQueue() joins the worker thread before
// BridgeContext destroys worker_conn.

template <typename Func>
void dispatch(sdbus::IConnection& conn,
              WorkQueue& queue,
              std::string object_path,
              Dart_Port_DL result_port,
              Func&& func) {
  queue.enqueue([&conn,
                 object_path = std::move(object_path),
                 result_port,
                 func = std::forward<Func>(func)]() mutable {
    try {
      auto proxy =
          sdbus::createProxy(conn, sdbus::ServiceName{kConnmanService},
                             sdbus::ObjectPath{object_path});
      TechnologyProxy tech(*proxy);
      func(tech);
      post_success(result_port, object_path);
    } catch (const sdbus::Error& e) {
      std::cerr << "TechnologyBridge (" << object_path << "): " << e.getName()
                << " — " << e.getMessage() << "\n";
      post_error(result_port, object_path, e);
    }
  });
}

}  // namespace

void TechnologyBridge::set_powered(sdbus::IConnection& conn,
                                   WorkQueue& queue,
                                   const std::string& object_path,
                                   bool powered,
                                   Dart_Port_DL result_port) {
  dispatch(conn, queue, object_path, result_port, [powered](auto& tech) {
    tech.SetProperty("Powered", sdbus::Variant{powered});
  });
}

void TechnologyBridge::scan(sdbus::IConnection& conn,
                            WorkQueue& queue,
                            const std::string& object_path,
                            Dart_Port_DL result_port) {
  dispatch(conn, queue, object_path, result_port,
           [](auto& tech) { tech.Scan(); });
}
