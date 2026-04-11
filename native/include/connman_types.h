// connman_types.h — wire types for all ConnMan-to-Dart payloads.
//
// Message discriminator byte at offset 0 in kExternalTypedData:
//   0x01 = ConnmanManagerProps      (Manager PropertiesChanged or initial state)
//   0x02 = ConnmanTechnologyProps   (Technology PropertiesChanged or initial state)
//   0x03 = ConnmanServiceProps      (Service PropertiesChanged or initial state)
//   0x04 = ConnmanServiceAdded      (ServicesAdded signal)
//   0x05 = ConnmanServiceRemoved    (ServicesRemoved signal)
//   0x06 = ConnmanTechnologyAdded   (TechnologyAdded signal)
//   0x07 = ConnmanTechnologyRemoved (TechnologyRemoved signal)
//   0x20 = ConnmanError             (method call failed)
//   0xFF = sentinel                 (stream done)

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "glaze_meta.h"

// ── Message discriminators ──────────────────────────────────────────────────

namespace connman::msg {
static constexpr uint8_t kManagerProps      = 0x01;
static constexpr uint8_t kTechnologyProps   = 0x02;
static constexpr uint8_t kServiceProps      = 0x03;
static constexpr uint8_t kServiceChanged    = 0x04;
static constexpr uint8_t kServiceRemoved    = 0x05;
static constexpr uint8_t kTechnologyAdded   = 0x06;
static constexpr uint8_t kTechnologyRemoved = 0x07;
static constexpr uint8_t kError             = 0x20;
static constexpr uint8_t kDone              = 0xFF;
}  // namespace connman::msg

// ── Manager properties ──────────────────────────────────────────────────────

struct ConnmanManagerProps {
  std::string state;
  bool offlineMode{};
  bool sessionMode{};
};
template <>
struct glz::meta<ConnmanManagerProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("state", &ConnmanManagerProps::state),
      glz::field("offlineMode", &ConnmanManagerProps::offlineMode),
      glz::field("sessionMode", &ConnmanManagerProps::sessionMode));
};

// ── Technology properties ───────────────────────────────────────────────────

struct ConnmanTechnologyProps {
  std::string objectPath;  // e.g. "/net/connman/technology/wifi"
  std::string name;
  std::string type;
  bool powered{};
  bool connected{};
  bool tethering{};
  std::string tetheringIdentifier;
  std::string tetheringPassphrase;
};
template <>
struct glz::meta<ConnmanTechnologyProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &ConnmanTechnologyProps::objectPath),
      glz::field("name", &ConnmanTechnologyProps::name),
      glz::field("type", &ConnmanTechnologyProps::type),
      glz::field("powered", &ConnmanTechnologyProps::powered),
      glz::field("connected", &ConnmanTechnologyProps::connected),
      glz::field("tethering", &ConnmanTechnologyProps::tethering),
      glz::field("tetheringIdentifier", &ConnmanTechnologyProps::tetheringIdentifier),
      glz::field("tetheringPassphrase", &ConnmanTechnologyProps::tetheringPassphrase));
};

// ── Service properties ──────────────────────────────────────────────────────

struct ConnmanServiceProps {
  std::string objectPath;  // e.g. "/net/connman/service/wifi_xyz"
  std::string name;
  std::string state;
  std::string type;
  int16_t strength{};
  bool favorite{};
  bool immutable{};
  bool autoConnect{};
  bool roaming{};
  std::vector<std::string> security;
  std::vector<std::string> nameservers;
  std::vector<std::string> domains;
};
template <>
struct glz::meta<ConnmanServiceProps> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &ConnmanServiceProps::objectPath),
      glz::field("name", &ConnmanServiceProps::name),
      glz::field("state", &ConnmanServiceProps::state),
      glz::field("type", &ConnmanServiceProps::type),
      glz::field("strength", &ConnmanServiceProps::strength),
      glz::field("favorite", &ConnmanServiceProps::favorite),
      glz::field("immutable", &ConnmanServiceProps::immutable),
      glz::field("autoConnect", &ConnmanServiceProps::autoConnect),
      glz::field("roaming", &ConnmanServiceProps::roaming),
      glz::field("security", &ConnmanServiceProps::security),
      glz::field("nameservers", &ConnmanServiceProps::nameservers),
      glz::field("domains", &ConnmanServiceProps::domains));
};

// ── Removed signal payload ──────────────────────────────────────────────────

struct ConnmanObjectRemoved {
  std::string objectPath;
};
template <>
struct glz::meta<ConnmanObjectRemoved> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &ConnmanObjectRemoved::objectPath));
};

// ── Error ───────────────────────────────────────────────────────────────────

struct ConnmanError {
  std::string objectPath;  // which object triggered the error
  std::string name;        // D-Bus error name
  std::string message;
};
template <>
struct glz::meta<ConnmanError> {
  static constexpr auto fields = std::make_tuple(
      glz::field("objectPath", &ConnmanError::objectPath),
      glz::field("name", &ConnmanError::name),
      glz::field("message", &ConnmanError::message));
};