# connman_native_comms

High-performance [Connection Manager (ConnMan)](https://git.kernel.org/pub/scm/network/connman/connman.git) client for Linux, built on a zero-copy Dart FFI ↔ C++ native bridge.

Inspired by and adapted from [jwinarske/bluez_native_comms](https://github.com/jwinarske/bluez_native_comms).

---

## Features

- **Zero-copy event delivery** — C++ posts Glaze BEVE binary frames directly to a Dart `ReceivePort`; no JSON serialisation on the hot path.
- **Full ConnMan API** — manage technologies (WiFi, Ethernet), scan for services, connect/disconnect, read and write properties.
- **Dart Streams** — `serviceAdded`, `serviceChanged`, `serviceRemoved`, `technologyChanged` streams keep your UI or daemon in sync automatically.
- **Flutter-ready** — ships a complete `flutter_network_manager` example app.
- **CLI examples** — four standalone Dart programs covering the most common use-cases.

---

## System dependencies

| Dependency | Ubuntu package |
|---|---|
| ConnMan daemon | `connman` |
| systemd D-Bus libraries | `libsystemd-dev` |
| expat (sdbus-cpp) | `libexpat1-dev` |
| CMake ≥ 3.21 | `cmake` |
| Ninja | `ninja-build` |
| Clang ≥ 19 | `clang-19` |

```bash
sudo apt-get install connman libsystemd-dev libexpat1-dev \
    cmake ninja-build clang-19
```

---

## Building the native library

```bash
# 1. Build the sdbus-c++ XML code-generator (one-time)
cmake -B build-codegen native/third_party/sdbus-cpp -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_C_COMPILER=clang-19 \
    -DSDBUSCPP_BUILD_CODEGEN=ON \
    -DSDBUSCPP_BUILD_LIBSYSTEMD=OFF \
    -DBUILD_SHARED_LIBS=OFF
cmake --build build-codegen --target sdbus-c++-xml2cpp --parallel

# 2. (Re-)generate D-Bus proxies
./scripts/generate_proxies.sh

# 3. Build the shared library
cmake -B build native/ -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_C_COMPILER=clang-19
cmake --build build --parallel
# → build/libconnman_nc.so
```

### Telling the Dart loader where to find the library

The loader checks locations in this order:

| Priority | Mechanism |
|---|---|
| 1 | `CONNMAN_NC_LIB` environment variable (full path to `.so`) |
| 2 | Directory of the running executable (`libconnman_nc.so`) |
| 3 | System library path (`DynamicLibrary.open('libconnman_nc.so')`) |

```bash
# Explicit path — useful during development
export CONNMAN_NC_LIB=/path/to/build/libconnman_nc.so
dart run example/scan_services.dart
```

---

## Installation

Add to your `pubspec.yaml`:

```yaml
dependencies:
  connman_native_comms: ^0.1.0
```

---

## Quick start

```dart
import 'package:connman_native_comms/connman_native_comms.dart';

Future<void> main() async {
  final client = ConnmanClient();
  await client.connect();

  // Print all known services
  for (final svc in client.services) {
    print('${svc.name.padRight(32)} ${svc.state}  rssi=${svc.strength}');
  }

  // Scan for new WiFi networks
  final wifi = client.technologies.firstWhere((t) => t.type == 'wifi');
  await wifi.scan();

  // React to changes
  client.serviceAdded.listen((svc) => print('+ ${svc.name}'));
  client.serviceChanged.listen((svc) => print('~ ${svc.name} → ${svc.state}'));
  
  // Provide credentials on demand
  client.agentRequestInput.listen((path) async {
    print('Password required for $path');
    // 1. Cache the password in the native agent
    client.setPassphrase(path, "my_secret_password");
    
    // 2. The first connect attempt was cancelled to ask for credentials. Retry it now!
    final service = client.services.firstWhere((s) => s.objectPath == path);
    await service.connect();
  });
  client.agentReportError.listen((err) => print('Password rejected for ${err.servicePath}: ${err.error}'));

  // Connect to a specific network
  final myNetwork = client.services.firstWhere((s) => s.name == 'MyNetwork');
  try {
    await myNetwork.connect();
  } catch (e) {
    // If a password is required, this initial attempt will throw.
    // The agentRequestInput listener will automatically catch the event and retry!
    print('Connection pending credential request...');
  }

  client.close();
}
```

---

## API reference

### `ConnmanClient`

| Member | Type | Description |
|---|---|---|
| `connect()` | `Future<void>` | Open the D-Bus connection and populate initial state. |
| `close()` | `void` | Destroy the native client and close all streams. |
| `services` | `List<ConnmanService>` | Snapshot of all known services. |
| `technologies` | `List<ConnmanTechnology>` | Snapshot of all known technologies. |
| `serviceAdded` | `Stream<ConnmanService>` | Fires when ConnMan reports a new service. |
| `serviceChanged` | `Stream<ConnmanService>` | Fires on any property change of an existing service. |
| `serviceRemoved` | `Stream<ConnmanService>` | Fires when a service disappears. |
| `technologyChanged` | `Stream<ConnmanTechnology>` | Fires on any property change of a technology. |
| `agentRequestInput` | `Stream<String>` | Fires when ConnMan requests a password for a service path. |
| `agentReportError` | `Stream<AgentErrorReport>` | Fires when ConnMan rejects a password. |
| `setPassphrase(String, String)` | `void` | Supply a Wi-Fi password to the native agent for a specific service. |
| `clearPassphrase(String)` | `void` | Clear a cached Wi-Fi password from the native agent. |

### `ConnmanService`

| Property | Type | Description |
|---|---|---|
| `objectPath` | `String` | D-Bus object path (unique key). |
| `name` | `String` | Human-readable SSID or network name. |
| `type` | `String` | `"wifi"`, `"ethernet"`, `"bluetooth"`, etc. |
| `state` | `String` | `"idle"`, `"association"`, `"configuration"`, `"ready"`, `"online"`, `"disconnect"`, `"failure"`. |
| `strength` | `int` | Signal strength 0–100. |
| `security` | `List<String>` | Security modes, e.g. `["psk"]`. |
| `favorite` | `bool` | Whether ConnMan has credentials saved. |
| `autoConnect` | `bool` | Whether ConnMan connects automatically. |
| `roaming` | `bool` | Roaming flag. |
| `nameservers` | `List<String>` | Assigned DNS servers. |
| `domains` | `List<String>` | Search domains. |

| Method | Returns | Description |
|---|---|---|
| `connect()` | `Future<void>` | Connect to this service. |
| `disconnect()` | `Future<void>` | Disconnect from this service. |

### `ConnmanTechnology`

| Property | Type | Description |
|---|---|---|
| `objectPath` | `String` | D-Bus object path (unique key). |
| `name` | `String` | Human-readable name, e.g. `"WiFi"`. |
| `type` | `String` | `"wifi"`, `"ethernet"`, etc. |
| `powered` | `bool` | Whether the technology is powered on. |
| `connected` | `bool` | Whether at least one service is connected. |
| `tethering` | `bool` | Whether tethering is active. |

| Method | Returns | Description |
|---|---|---|
| `setPowered(bool)` | `Future<void>` | Power the technology on or off. |
| `scan()` | `Future<void>` | Trigger an active scan (WiFi only). |

---

## Notification architecture

```
ConnMan daemon (D-Bus)
        │
        │  sdbus-cpp async reply / signal
        ▼
  C++ BridgeContext
  ┌─────────────────────────────────────────────────────────┐
  │  ServiceBridge / TechnologyBridge                       │
  │    └─ glaze::write_binary<T>(payload)                   │
  │    └─ Dart_PostCObject_DL(port, CObject{binary, len})   │
  └─────────────────────────────────────────────────────────┘
        │  zero-copy binary frame (Glaze BEVE)
        ▼
  Dart ReceivePort (isolate-safe)
        │
        │  GlazeCodec.decodePayload(Uint8List)
        ▼
  ConnmanClient._dispatch()
        │
  ┌─────┴─────────────────────────────────┐
  │ serviceAdded / serviceChanged /        │
  │ serviceRemoved / technologyChanged     │
  └────────────────────────────────────────┘
```

The discriminator byte at `message[0]` selects the decode path:

| Byte | Payload type |
|---|---|
| `0x01` | `ConnmanManagerProps` |
| `0x02` | `ConnmanTechnology` |
| `0x03` | `ConnmanService` (added) |
| `0x04` | `ConnmanService` (changed) |
| `0x05` | `ConnmanService` (removed) |
| `0x06` | Technology changed |
| `0x07` | Technology removed |
| `['AgentRequestInput', path]` | C++ to Dart agent string array |
| `['AgentReportError', path, err]` | C++ to Dart agent string array |
| `0x20` | Operation success |
| `0xFF` | Error string |

---

## CLI examples

| Example | Description |
|---|---|
| [`example/scan_services.dart`](example/scan_services.dart) | Scan for WiFi networks and print them sorted by signal strength. |
| [`example/connect_service.dart`](example/connect_service.dart) | Connect to a WiFi network by SSID. |
| [`example/read_service.dart`](example/read_service.dart) | Print all properties of a named service. |
| [`example/monitor_service.dart`](example/monitor_service.dart) | Monitor state changes of a service in real time. |

```bash
# Scan
dart run example/scan_services.dart

# Connect (with optional timeout)
dart run example/connect_service.dart --ssid MyNetwork --timeout 30

# Read properties
dart run example/read_service.dart --ssid MyNetwork

# Monitor
dart run example/monitor_service.dart --ssid MyNetwork
```

---

## Flutter example

`example/flutter_network_manager/` is a full Flutter application that demonstrates:

- Live WiFi network scanner with signal-strength icons.
- Per-service detail screen with connect / disconnect.
- Technology detail screen with power toggle and scan.
- Auto-pop navigation when a service disconnects or WiFi is powered off.

```bash
cd example/flutter_network_manager
flutter pub get
flutter run
```

> Make sure `CONNMAN_NC_LIB` is set or `libconnman_nc.so` is on the library path before launching.

---

## Running the tests

```bash
# Dart unit tests
dart test

# C++ unit tests
cmake -B build native/ -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_C_COMPILER=clang-19 \
    -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -j4
```

### Coverage

```bash
./scripts/coverage.sh          # → build-cov/coverage.info
```

### ASAN + UBSan

```bash
./scripts/asan.sh              # → build-asan/
```

---

## Contributing

1. Fork and create a feature branch.
2. Run `dart format lib/ test/ example/` and `./scripts/clang_format.sh fix` before committing.
3. Ensure `dart analyze --fatal-infos` and `ctest` pass.
4. Open a pull request against `master`.

Issues and pull requests are welcome at the project repository.

---

## License

See [LICENSE](LICENSE).
