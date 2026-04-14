// example/monitor_service.dart
// Demonstrates the zero-copy Stream<ConnmanService> notification path.
//
// Subscribes to serviceChanged and prints every state transition for a given
// SSID.  State updates arrive directly from the sdbus-cpp event loop thread
// via Dart_PostCObject_DL — no polling required.
//
// Usage:
//   dart run example/monitor_service.dart <ssid> [--timeout <seconds>]

import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';

import 'example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    print('Usage: dart run example/monitor_service.dart <ssid> '
        '[--timeout <seconds>]');
    return;
  }

  final ssid = args[0];
  final timeout = parseScanTimeout(args);

  final client = ConnmanClient();
  await client.connect();

  final wifi = client.technologies.where((t) => t.type == 'wifi').firstOrNull;
  if (wifi == null) {
    print('No WiFi technology found.');
    await client.close();
    return;
  }

  if (!wifi.powered) {
    print('Powering on WiFi...');
    await wifi.setPowered(true);
    await Future<void>.delayed(const Duration(milliseconds: 500));
  }

  final service = await findService(client, wifi, ssid: ssid, timeout: timeout);
  if (service == null) {
    await client.close();
    return;
  }

  print('Monitoring "${service.name}" for ${timeout.inSeconds} seconds...');
  print('Current state: ${service.state}\n');

  // Zero-copy Stream<ConnmanService> — state updates arrive directly from
  // the sdbus-cpp event loop thread via Dart_PostCObject_DL.
  var count = 0;
  final sub = client.serviceChanged.where((s) => s.name == ssid).listen((s) {
    count++;
    print('[$count] state: ${s.state.padRight(12)}'
        '  strength: ${s.strength}'
        '  autoConnect: ${s.autoConnect}');
  });

  await Future<void>.delayed(timeout);
  await sub.cancel();

  print('\nReceived $count state update(s).');
  await client.close();
  print('Done.');
}
