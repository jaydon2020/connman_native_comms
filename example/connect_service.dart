// example/connect_service.dart — connect to a WiFi network by SSID.
//
// Usage:
//   dart run example/connect_service.dart <ssid> [--timeout <seconds>]

import 'package:connman_native_comms/connman_native_comms.dart';

import 'example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    print('Usage: dart run example/connect_service.dart <ssid> '
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

  print('Connecting to "${service.name}"...');

  const maxRetries = 3;
  for (var attempt = 1; attempt <= maxRetries; attempt++) {
    try {
      await service.connect();
      break;
    } on ConnmanException catch (e) {
      print('Attempt $attempt failed: $e');
      if (attempt == maxRetries) {
        print('Giving up after $maxRetries attempts.');
        await client.close();
        return;
      }
      await Future<void>.delayed(const Duration(seconds: 1));
    }
  }

  print('State:       ${service.state}');
  print('Type:        ${service.type}');
  print('Security:    ${service.security}');
  print('Strength:    ${service.strength}');
  print('AutoConnect: ${service.autoConnect}');
  print('Nameservers: ${service.nameservers}');
  print('Domains:     ${service.domains}');

  await client.close();
  print('Done.');
}
