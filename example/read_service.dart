// example/read_service.dart — read and print all properties of a service.
//
// Usage:
//   dart run example/read_service.dart <ssid> [--timeout <seconds>]

import 'package:connman_native_comms/connman_native_comms.dart';

import 'example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    print('Usage: dart run example/read_service.dart <ssid> '
        '[--timeout <seconds>]');
    return;
  }

  final ssid = args[0];
  final timeout = parseScanTimeout(args);

  final client = ConnmanClient();
  await client.connect();

  final wifi =
      client.technologies.where((t) => t.type == 'wifi').firstOrNull;
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

  final service =
      await findService(client, wifi, ssid: ssid, timeout: timeout);
  if (service == null) {
    await client.close();
    return;
  }

  // Print all service properties.
  print('Object path:  ${service.objectPath}');
  print('Name:         ${service.name}');
  print('State:        ${service.state}');
  print('Type:         ${service.type}');
  print('Strength:     ${service.strength}');
  print('Favorite:     ${service.favorite}');
  print('Immutable:    ${service.immutable}');
  print('AutoConnect:  ${service.autoConnect}');
  print('Roaming:      ${service.roaming}');
  print('Security:     ${service.security}');
  print('Nameservers:  ${service.nameservers}');
  print('Domains:      ${service.domains}');

  await client.close();
  print('Done.');
}
