// test_connman_manager.cpp — Unit tests for ConnmanManager property extraction
// and posting logic.
//
// These tests verify the static property extraction methods that convert
// sdbus::Variant property maps into ConnMan type structs.

#include "connman_manager.h"

#include <gtest/gtest.h>

using PropertiesMap = std::map<std::string, sdbus::Variant>;

// ── Manager property extraction ─────────────────────────────────────────────

TEST(ConnmanManagerExtract, ManagerPropsBasic) {
  PropertiesMap props;
  props["State"] = sdbus::Variant{std::string{"online"}};
  props["OfflineMode"] = sdbus::Variant{false};
  props["SessionMode"] = sdbus::Variant{true};

  auto m = ConnmanManager::extract_manager_props(props);

  EXPECT_EQ(m.state, "online");
  EXPECT_FALSE(m.offlineMode);
  EXPECT_TRUE(m.sessionMode);
}

TEST(ConnmanManagerExtract, ManagerPropsMissing) {
  PropertiesMap props;
  auto m = ConnmanManager::extract_manager_props(props);

  EXPECT_TRUE(m.state.empty());
  EXPECT_FALSE(m.offlineMode);
  EXPECT_FALSE(m.sessionMode);
}

// ── Technology property extraction ──────────────────────────────────────────

TEST(ConnmanManagerExtract, TechnologyPropsBasic) {
  PropertiesMap props;
  props["Name"] = sdbus::Variant{std::string{"WiFi"}};
  props["Type"] = sdbus::Variant{std::string{"wifi"}};
  props["Powered"] = sdbus::Variant{true};
  props["Connected"] = sdbus::Variant{false};
  props["Tethering"] = sdbus::Variant{true};
  props["TetheringIdentifier"] = sdbus::Variant{std::string{"MyHotspot"}};
  props["TetheringPassphrase"] = sdbus::Variant{std::string{"secret"}};

  auto t = ConnmanManager::extract_technology_props(
      "/net/connman/technology/wifi", props);

  EXPECT_EQ(t.objectPath, "/net/connman/technology/wifi");
  EXPECT_EQ(t.name, "WiFi");
  EXPECT_EQ(t.type, "wifi");
  EXPECT_TRUE(t.powered);
  EXPECT_FALSE(t.connected);
  EXPECT_TRUE(t.tethering);
  EXPECT_EQ(t.tetheringIdentifier, "MyHotspot");
  EXPECT_EQ(t.tetheringPassphrase, "secret");
}

// ── Service property extraction ─────────────────────────────────────────────

TEST(ConnmanManagerExtract, ServicePropsBasic) {
  PropertiesMap props;
  props["Name"] = sdbus::Variant{std::string{"Home WiFi"}};
  props["State"] = sdbus::Variant{std::string{"ready"}};
  props["Type"] = sdbus::Variant{std::string{"wifi"}};
  props["Strength"] = sdbus::Variant{uint8_t{85}};
  props["Favorite"] = sdbus::Variant{true};
  props["Immutable"] = sdbus::Variant{false};
  props["AutoConnect"] = sdbus::Variant{true};
  props["Roaming"] = sdbus::Variant{false};
  props["Security"] = sdbus::Variant{std::vector<std::string>{"psk", "wep"}};
  props["Nameservers"] =
      sdbus::Variant{std::vector<std::string>{"8.8.8.8", "8.8.4.4"}};
  props["Domains"] = sdbus::Variant{std::vector<std::string>{"local"}};

  auto s = ConnmanManager::extract_service_props(
      "/net/connman/service/wifi_xyz", props);

  EXPECT_EQ(s.objectPath, "/net/connman/service/wifi_xyz");
  EXPECT_EQ(s.name, "Home WiFi");
  EXPECT_EQ(s.state, "ready");
  EXPECT_EQ(s.type, "wifi");
  EXPECT_EQ(s.strength, 85);
  EXPECT_TRUE(s.favorite);
  EXPECT_FALSE(s.immutable);
  EXPECT_TRUE(s.autoConnect);
  EXPECT_FALSE(s.roaming);

  ASSERT_EQ(s.security.size(), 2u);
  EXPECT_EQ(s.security[0], "psk");

  ASSERT_EQ(s.nameservers.size(), 2u);
  EXPECT_EQ(s.nameservers[0], "8.8.8.8");

  ASSERT_EQ(s.domains.size(), 1u);
  EXPECT_EQ(s.domains[0], "local");
}

TEST(ConnmanManagerExtract, ServicePropsStrengthCoercion) {
  PropertiesMap props;
  // SDBus variant types can be finicky. Test that missing strength defaults safely.
  auto s = ConnmanManager::extract_service_props(
      "/net/connman/service/wifi_xyz", props);
  EXPECT_EQ(s.strength, 0);

  // Test what happens if someone passes an int16 instead of uint8_t by
  // accident? The sdbus::Variant getter throws, and we should catch it and
  // default to 0.
  props["Strength"] = sdbus::Variant{int16_t{50}};
  auto s2 = ConnmanManager::extract_service_props(
      "/net/connman/service/wifi_xyz2", props);
  EXPECT_EQ(s2.strength, 0);
}

// ── Manager props: wrong-type variant falls back ────────────────────────────

TEST(ConnmanManagerExtract, ManagerPropsWrongTypeFallsBack) {
  PropertiesMap props;
  // State expects std::string but receives bool — get_prop should catch and
  // return the empty-string fallback.
  props["State"] = sdbus::Variant{true};
  props["OfflineMode"] = sdbus::Variant{std::string{"not-a-bool"}};

  auto m = ConnmanManager::extract_manager_props(props);

  EXPECT_TRUE(m.state.empty());
  EXPECT_FALSE(m.offlineMode);
}

// ── Technology props: missing fields fall back to defaults ───────────────────

TEST(ConnmanManagerExtract, TechnologyPropsMissing) {
  PropertiesMap props;
  auto t = ConnmanManager::extract_technology_props(
      "/net/connman/technology/wifi", props);

  EXPECT_EQ(t.objectPath, "/net/connman/technology/wifi");
  EXPECT_TRUE(t.name.empty());
  EXPECT_TRUE(t.type.empty());
  EXPECT_FALSE(t.powered);
  EXPECT_FALSE(t.connected);
  EXPECT_FALSE(t.tethering);
  EXPECT_TRUE(t.tetheringIdentifier.empty());
  EXPECT_TRUE(t.tetheringPassphrase.empty());
}

// ── Technology props: wrong-type variant falls back ──────────────────────────

TEST(ConnmanManagerExtract, TechnologyPropsWrongTypeFallsBack) {
  PropertiesMap props;
  // Powered expects bool but receives string.
  props["Powered"] = sdbus::Variant{std::string{"yes"}};
  // Name expects string but receives bool.
  props["Name"] = sdbus::Variant{true};

  auto t = ConnmanManager::extract_technology_props(
      "/net/connman/technology/ethernet", props);

  EXPECT_FALSE(t.powered);
  EXPECT_TRUE(t.name.empty());
}

// ── Service props: objectPath comes from argument, not props map ─────────────

TEST(ConnmanManagerExtract, ServicePropsObjectPathFromArgument) {
  PropertiesMap props;
  // Props map has no "ObjectPath" key — path must come exclusively from the arg.
  props["Name"] = sdbus::Variant{std::string{"Test"}};

  auto s = ConnmanManager::extract_service_props(
      "/net/connman/service/eth_aabbcc", props);

  EXPECT_EQ(s.objectPath, "/net/connman/service/eth_aabbcc");
}

// ── Service props: wrong-type bool variant falls back ────────────────────────

TEST(ConnmanManagerExtract, ServicePropsWrongTypeBoolFallsBack) {
  PropertiesMap props;
  // All bool fields receive a string — each should fall back to false.
  props["Favorite"]    = sdbus::Variant{std::string{"true"}};
  props["Immutable"]   = sdbus::Variant{std::string{"true"}};
  props["AutoConnect"] = sdbus::Variant{std::string{"true"}};
  props["Roaming"]     = sdbus::Variant{std::string{"true"}};

  auto s = ConnmanManager::extract_service_props(
      "/net/connman/service/wifi_xyz", props);

  EXPECT_FALSE(s.favorite);
  EXPECT_FALSE(s.immutable);
  EXPECT_FALSE(s.autoConnect);
  EXPECT_FALSE(s.roaming);
}

// ── Service props: Strength zero is valid, distinct from missing ─────────────

TEST(ConnmanManagerExtract, ServicePropsStrengthZeroIsValid) {
  PropertiesMap props;
  props["Strength"] = sdbus::Variant{uint8_t{0}};

  auto s = ConnmanManager::extract_service_props(
      "/net/connman/service/wifi_xyz", props);

  // 0 is a legitimate signal reading, not a fallback sentinel.
  EXPECT_EQ(s.strength, 0);
}

// ── get_prop: direct hit returns value, not fallback ────────────────────────

TEST(ConnmanManagerExtract, GetPropHitReturnValue) {
  PropertiesMap props;
  props["Key"] = sdbus::Variant{std::string{"value"}};

  auto result = ConnmanManager::get_prop<std::string>(props, "Key", "default");
  EXPECT_EQ(result, "value");
}

TEST(ConnmanManagerExtract, GetPropMissReturnsFallback) {
  PropertiesMap props;

  auto result = ConnmanManager::get_prop<std::string>(props, "Missing", "default");
  EXPECT_EQ(result, "default");
}

TEST(ConnmanManagerExtract, GetPropWrongTypeReturnsFallback) {
  PropertiesMap props;
  props["Key"] = sdbus::Variant{true};  // bool, not string

  auto result = ConnmanManager::get_prop<std::string>(props, "Key", "default");
  EXPECT_EQ(result, "default");
}

// ── Roundtrip: extract → encode → decode (manager props) ────────────────────

TEST(ConnmanManagerExtract, ManagerExtractEncodeDecodeRoundtrip) {
  PropertiesMap props;
  props["State"]       = sdbus::Variant{std::string{"online"}};
  props["OfflineMode"] = sdbus::Variant{true};
  props["SessionMode"] = sdbus::Variant{false};

  auto m = ConnmanManager::extract_manager_props(props);
  auto buf = glz::encode(m);
  ConnmanManagerProps decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.state, "online");
  EXPECT_TRUE(decoded.offlineMode);
  EXPECT_FALSE(decoded.sessionMode);
}

// ── Roundtrip: extract → encode → decode ────────────────────────────────────

TEST(ConnmanManagerExtract, TechnologyExtractEncodeDecodeRoundtrip) {
  PropertiesMap props;
  props["Name"] = sdbus::Variant{std::string{"Bluetooth"}};
  props["Type"] = sdbus::Variant{std::string{"bluetooth"}};
  props["Powered"] = sdbus::Variant{true};

  auto t = ConnmanManager::extract_technology_props(
      "/net/connman/technology/bluetooth", props);
  auto buf = glz::encode(t);
  ConnmanTechnologyProps decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.objectPath, "/net/connman/technology/bluetooth");
  EXPECT_EQ(decoded.name, "Bluetooth");
  EXPECT_TRUE(decoded.powered);
}

TEST(ConnmanManagerExtract, ServiceExtractEncodeDecodeRoundtrip) {
  PropertiesMap props;
  props["Name"] = sdbus::Variant{std::string{"Starbucks WiFi"}};
  props["Strength"] = sdbus::Variant{uint8_t{42}};
  props["Security"] = sdbus::Variant{std::vector<std::string>{"none"}};

  auto s = ConnmanManager::extract_service_props(
      "/net/connman/service/wifi_sbux", props);
  auto buf = glz::encode(s);
  ConnmanServiceProps decoded;
  glz::decode(buf.data(), 0, decoded);

  EXPECT_EQ(decoded.objectPath, "/net/connman/service/wifi_sbux");
  EXPECT_EQ(decoded.name, "Starbucks WiFi");
  EXPECT_EQ(decoded.strength, 42);
  ASSERT_EQ(decoded.security.size(), 1u);
  EXPECT_EQ(decoded.security[0], "none");
}