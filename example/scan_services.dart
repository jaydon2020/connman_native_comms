// example/scan_services.dart — scan WiFi and print discovered services.
//
// Usage:
//   dart run example/scan_services.dart [--timeout <seconds>]

import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';

import 'example_utils.dart';

Future<void> main(List<String> args) async {
  final timeout = parseScanTimeout(args);

  final client = ConnmanClient();
  await client.connect();

  // Find the WiFi technology.
  final wifi =
      client.technologies.where((t) => t.type == 'wifi').firstOrNull;
  if (wifi == null) {
    print('No WiFi technology found.');
    await client.close();
    return;
  }

  print('Technology: ${wifi.name}  (powered: ${wifi.powered})');

  if (!wifi.powered) {
    print('Powering on WiFi...');
    await wifi.setPowered(true);
    // Give ConnMan a moment to bring the technology up.
    await Future<void>.delayed(const Duration(milliseconds: 500));
  }

  // Subscribe before scanning so no results are missed.
  void printService(ConnmanService service) {
    if (service.type != 'wifi') return;
    final name = service.name.isNotEmpty ? service.name : '(hidden)';
    final sec = service.security.isNotEmpty
        ? '  [${service.security.join(', ')}]'
        : '  [open]';
    print('strength: ${service.strength.toString().padLeft(3)}'
        '  $name$sec');
  }

  final subAdded = client.serviceAdded.listen(printService);
  final subChanged = client.serviceChanged.listen(printService);

  print('Scanning for ${timeout.inSeconds} seconds...\n');
  await wifi.scan();

  await Future<void>.delayed(timeout);
  await subAdded.cancel();
  await subChanged.cancel();

  // Summary: all known WiFi services sorted by signal strength.
  final wifiServices = client.services
      .where((s) => s.type == 'wifi')
      .toList()
    ..sort((a, b) => b.strength.compareTo(a.strength));

  if (wifiServices.isNotEmpty) {
    print('\n── Known services ─────────────────────────────────────────────');
    for (final svc in wifiServices) {
      final name = svc.name.isNotEmpty ? svc.name : '(hidden)';
      print('  ${svc.state.padRight(12)}'
          '  strength: ${svc.strength.toString().padLeft(3)}'
          '  $name');
    }
  }

  await client.close();
}
