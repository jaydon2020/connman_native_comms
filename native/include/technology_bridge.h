// technology_bridge.h — async C++ bridge for the net.connman.Technology
// D-Bus interface.
//
// Each static method dispatches its D-Bus call on a detached thread and posts
// the result — ConnmanMethodSuccess (0xFF) or ConnmanError (0x20) — back to
// Dart via result_port.

#pragma once

#include <string>

#include "dart_api_dl.h"

class TechnologyBridge {
 private:
  static constexpr auto kTechnologyIface = "net.connman.Technology";

 public:
  // Set the "Powered" property on the given technology object.
  static void set_powered(const std::string& object_path,
                          bool powered,
                          Dart_Port_DL result_port);

  // Trigger a service scan on the given technology object.
  static void scan(const std::string& object_path, Dart_Port_DL result_port);
};
