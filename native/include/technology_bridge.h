// technology_bridge.h — async C++ bridge for the net.connman.Technology
// D-Bus interface.
//
// Each static method enqueues its D-Bus call on the shared WorkQueue and posts
// the result — ConnmanMethodSuccess (0xFF) or ConnmanError (0x20) — back to
// Dart via result_port.  All calls share a single sdbus::IConnection owned by
// BridgeContext, eliminating the per-call connection overhead of the original
// std::thread::detach() pattern.

#pragma once

#include <string>

#include <sdbus-c++/sdbus-c++.h>

#include "dart_api_dl.h"
#include "work_queue.h"

class TechnologyBridge {
 private:
  static constexpr auto kTechnologyIface = "net.connman.Technology";

 public:
  // Set the "Powered" property on the given technology object.
  static void set_powered(sdbus::IConnection& conn,
                          WorkQueue& queue,
                          const std::string& object_path,
                          bool powered,
                          Dart_Port_DL result_port);

  // Trigger a service scan on the given technology object.
  static void scan(sdbus::IConnection& conn,
                   WorkQueue& queue,
                   const std::string& object_path,
                   Dart_Port_DL result_port);
};
