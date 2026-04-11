// test_types.cpp — glaze encode/decode roundtrip tests for every Connman struct.

#include "connman_types.h"

#include <gtest/gtest.h>

// ── ConnmanManagerProps ─────────────────────────────────────────────────────

TEST(ConnmanTypes, ManagerPropsRoundtrip) {
  ConnmanManagerProps orig;
  orig.state = "online";
  orig.offlineMode = false;
  orig.sessionMode = true;

  auto buf = glz::encode(orig);
  ConnmanManagerProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.state, orig.state);
  EXPECT_EQ(decoded.offlineMode, orig.offlineMode);
  EXPECT_EQ(decoded.sessionMode, orig.sessionMode);
}

TEST(ConnmanTypes, ManagerPropsDefaultsRoundtrip) {
  ConnmanManagerProps orig;  // all defaults

  auto buf = glz::encode(orig);
  ConnmanManagerProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_TRUE(decoded.state.empty());
  EXPECT_FALSE(decoded.offlineMode);
  EXPECT_FALSE(decoded.sessionMode);
}

// ── ConnmanTechnologyProps ──────────────────────────────────────────────────

TEST(ConnmanTypes, TechnologyPropsRoundtrip) {
  ConnmanTechnologyProps orig;
  orig.objectPath = "/net/connman/technology/wifi";
  orig.name = "WiFi";
  orig.type = "wifi";
  orig.powered = true;
  orig.connected = true;
  orig.tethering = false;
  orig.tetheringIdentifier = "MyConnManAP";
  orig.tetheringPassphrase = "supersecret";

  auto buf = glz::encode(orig);
  ConnmanTechnologyProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
  EXPECT_EQ(decoded.name, orig.name);
  EXPECT_EQ(decoded.type, orig.type);
  EXPECT_EQ(decoded.powered, orig.powered);
  EXPECT_EQ(decoded.connected, orig.connected);
  EXPECT_EQ(decoded.tethering, orig.tethering);
  EXPECT_EQ(decoded.tetheringIdentifier, orig.tetheringIdentifier);
  EXPECT_EQ(decoded.tetheringPassphrase, orig.tetheringPassphrase);
}

TEST(ConnmanTypes, TechnologyPropsTetheringRoundtrip) {
  ConnmanTechnologyProps orig;
  orig.tethering = true;
  orig.tetheringIdentifier = "";
  orig.tetheringPassphrase = "";

  auto buf = glz::encode(orig);
  ConnmanTechnologyProps decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_TRUE(decoded.tethering);
  EXPECT_TRUE(decoded.tetheringIdentifier.empty());
  EXPECT_TRUE(decoded.tetheringPassphrase.empty());
}

TEST(ConnmanTypes, TechnologyPropsDefaultsRoundtrip) {
  ConnmanTechnologyProps orig;

  auto buf = glz::encode(orig);
  ConnmanTechnologyProps decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_TRUE(decoded.objectPath.empty());
  EXPECT_TRUE(decoded.name.empty());
  EXPECT_TRUE(decoded.type.empty());
  EXPECT_FALSE(decoded.powered);
  EXPECT_FALSE(decoded.connected);
  EXPECT_FALSE(decoded.tethering);
  EXPECT_TRUE(decoded.tetheringIdentifier.empty());
  EXPECT_TRUE(decoded.tetheringPassphrase.empty());
}

// ── ConnmanServiceProps ─────────────────────────────────────────────────────

TEST(ConnmanTypes, ServicePropsRoundtrip) {
  ConnmanServiceProps orig;
  orig.objectPath = "/net/connman/service/wifi_123456_managed_psk";
  orig.name = "MyHomeNetwork";
  orig.state = "ready";
  orig.type = "wifi";
  orig.strength = 85;
  orig.favorite = true;
  orig.immutable = false;
  orig.autoConnect = true;
  orig.roaming = false;
  orig.security = {"psk", "wps"};
  orig.nameservers = {"8.8.8.8", "8.8.4.4"};
  orig.domains = {"localdomain"};

  auto buf = glz::encode(orig);
  ConnmanServiceProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
  EXPECT_EQ(decoded.name, orig.name);
  EXPECT_EQ(decoded.state, orig.state);
  EXPECT_EQ(decoded.type, orig.type);
  EXPECT_EQ(decoded.strength, orig.strength);
  EXPECT_EQ(decoded.favorite, orig.favorite);
  EXPECT_EQ(decoded.immutable, orig.immutable);
  EXPECT_EQ(decoded.autoConnect, orig.autoConnect);
  EXPECT_EQ(decoded.roaming, orig.roaming);
  EXPECT_EQ(decoded.security, orig.security);
  EXPECT_EQ(decoded.nameservers, orig.nameservers);
  EXPECT_EQ(decoded.domains, orig.domains);
}

TEST(ConnmanTypes, ServicePropsEmptyVectors) {
  ConnmanServiceProps orig;
  orig.objectPath = "/net/connman/service/ethernet_AA_BB";
  orig.name = "Wired";
  orig.state = "online";
  orig.type = "ethernet";
  // Vectors explicitly left empty

  auto buf = glz::encode(orig);
  ConnmanServiceProps decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_TRUE(decoded.security.empty());
  EXPECT_TRUE(decoded.nameservers.empty());
  EXPECT_TRUE(decoded.domains.empty());
}

TEST(ConnmanTypes, ServicePropsNegativeStrength) {
  ConnmanServiceProps orig;
  orig.strength = -42; // Testing edge case where it might be expressed as dBm negatively

  auto buf = glz::encode(orig);
  ConnmanServiceProps decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.strength, -42);
}

TEST(ConnmanTypes, ServicePropsLargePayload) {
  ConnmanServiceProps orig;
  // Simulating 512 different domains mapping to maximum IP allocations
  orig.domains.resize(512, "localdomain");

  auto buf = glz::encode(orig);
  ConnmanServiceProps decoded;
  glz::decode(buf.data(), 0, decoded);

  ASSERT_EQ(decoded.domains.size(), 512u);
  EXPECT_EQ(decoded.domains[500], "localdomain");
}

// ── ConnmanObjectRemoved ────────────────────────────────────────────────────

TEST(ConnmanTypes, ObjectRemovedRoundtrip) {
  ConnmanObjectRemoved orig;
  orig.objectPath = "/net/connman/service/wifi_123";

  auto buf = glz::encode(orig);
  ConnmanObjectRemoved decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
}

// ── ConnmanError ────────────────────────────────────────────────────────────

TEST(ConnmanTypes, ErrorRoundtrip) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/service/wifi_123456_managed_psk";
  orig.name = "net.connman.Error.Failed";
  orig.message = "Invalid passphrase";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
  EXPECT_EQ(decoded.name, orig.name);
  EXPECT_EQ(decoded.message, orig.message);
}

TEST(ConnmanTypes, ErrorEmptyMessage) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/manager";
  orig.name = "net.connman.Error.NotRegistered";
  orig.message = "";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
  EXPECT_EQ(decoded.name, orig.name);
  EXPECT_TRUE(decoded.message.empty());
}

// ── ConnmanError: D-Bus error name variants ──────────────────────────────────

TEST(ConnmanTypes, ErrorInvalidArguments) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/technology/wifi";
  orig.name       = "net.connman.Error.InvalidArguments";
  orig.message    = "Wrong parameter type for Powered";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.name, "net.connman.Error.InvalidArguments");
  EXPECT_FALSE(decoded.message.empty());
}

TEST(ConnmanTypes, ErrorServiceAlreadyConnected) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/service/wifi_home_managed_psk";
  orig.name       = "net.connman.Error.AlreadyConnected";
  orig.message    = "Already connected";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.name, "net.connman.Error.AlreadyConnected");
}

TEST(ConnmanTypes, ErrorAuthenticationFailure) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/service/wifi_home_managed_psk";
  orig.name       = "net.connman.Error.Failed";
  orig.message    = "connect failed: Input/output error";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.name, "net.connman.Error.Failed");
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
}

TEST(ConnmanTypes, ErrorPropertyNotFound) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/service/ethernet_wired";
  orig.name       = "net.connman.Error.InvalidProperty";
  orig.message    = "No such property";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.name, "net.connman.Error.InvalidProperty");
}

TEST(ConnmanTypes, ErrorTimeout) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/service/wifi_open";
  orig.name       = "net.connman.Error.OperationTimeout";
  orig.message    = "Operation timed out";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.name, "net.connman.Error.OperationTimeout");
  EXPECT_EQ(decoded.message, "Operation timed out");
}

TEST(ConnmanTypes, ErrorUnknownWithContext) {
  ConnmanError orig;
  orig.objectPath = "/net/connman/technology/bluetooth";
  orig.name       = "org.freedesktop.DBus.Error.UnknownMethod";
  orig.message    = "Method Scan not found";

  auto buf = glz::encode(orig);
  ConnmanError decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.name, "org.freedesktop.DBus.Error.UnknownMethod");
  EXPECT_EQ(decoded.objectPath, "/net/connman/technology/bluetooth");
}

// ── ConnmanMethodSuccess ─────────────────────────────────────────────────────

TEST(ConnmanTypes, MethodSuccessRoundtrip) {
  ConnmanMethodSuccess orig;
  orig.objectPath = "/net/connman/service/wifi_home_managed_psk";

  auto buf = glz::encode(orig);
  ConnmanMethodSuccess decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_EQ(decoded.objectPath, orig.objectPath);
}

TEST(ConnmanTypes, MethodSuccessTechnologyPath) {
  ConnmanMethodSuccess orig;
  orig.objectPath = "/net/connman/technology/wifi";

  auto buf = glz::encode(orig);
  ConnmanMethodSuccess decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.objectPath, "/net/connman/technology/wifi");
}

TEST(ConnmanTypes, MethodSuccessEmptyPath) {
  ConnmanMethodSuccess orig;  // default — empty path

  auto buf = glz::encode(orig);
  ConnmanMethodSuccess decoded;
  auto end = glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(end, buf.size());
  EXPECT_TRUE(decoded.objectPath.empty());
}