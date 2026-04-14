// service_bridge.h — async C++ bridge for the net.connman.Service D-Bus
// interface.
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

class ServiceBridge {
 private:
  static constexpr auto kServiceIface = "net.connman.Service";

 public:
  // Associate with the network and obtain an IP address.
  static void connect(sdbus::IConnection& conn,
                      WorkQueue& queue,
                      const std::string& object_path,
                      Dart_Port_DL result_port);

  // Disconnect from the network.
  static void disconnect(sdbus::IConnection& conn,
                         WorkQueue& queue,
                         const std::string& object_path,
                         Dart_Port_DL result_port);

  // Remove (forget) this service entry.
  static void remove(sdbus::IConnection& conn,
                     WorkQueue& queue,
                     const std::string& object_path,
                     Dart_Port_DL result_port);

  // Set the AutoConnect property.
  static void set_auto_connect(sdbus::IConnection& conn,
                               WorkQueue& queue,
                               const std::string& object_path,
                               bool auto_connect,
                               Dart_Port_DL result_port);

  // Set the IPv4.Configuration property.
  // method: "dhcp" | "manual" | "off"
  // address, netmask, gateway are ignored when method is "dhcp" or "off".
  static void set_ipv4_config(sdbus::IConnection& conn,
                              WorkQueue& queue,
                              const std::string& object_path,
                              const std::string& method,
                              const std::string& address,
                              const std::string& netmask,
                              const std::string& gateway,
                              Dart_Port_DL result_port);
};
