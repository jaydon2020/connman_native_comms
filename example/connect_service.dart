// example/connect_service.dart — connect to a WiFi network by SSID.
//
// Usage:
//   dart run example/connect_service.dart <ssid> [--timeout <seconds>]

import 'dart:async';
import 'dart:io';
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
  
  // Listen for passphrase requests BEFORE connecting.
  client.agentRequestInput.listen((path) {
    print('\n[Agent] Passphrase requested for $path');
    stdout.write('Enter password: ');
    // Hide input if possible, but keep it simple for the example.
    final pass = stdin.readLineSync() ?? '';
    client.agentSetPassphrase(path, pass);
  });

  client.agentReportError.listen((error) {
    print('\n[Agent] Error for ${error.$1}: ${error.$2}');
  });

  try {
    await client.connect();
  } catch (e) {
    print('Failed to connect to ConnMan: $e');
    return;
  }

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
    print('Service "$ssid" not found.');
    await client.close();
    return;
  }

  print('Connecting to "${service.name}" (${service.objectPath})...');

  try {
    // service.connect() in the library now handles InProgress gracefully.
    await service.connect();
  } on ConnmanOperationAbortedException {
    print('  Operation aborted. Stale credentials might be present.');
    print('  Removing service and retrying with fresh authentication...');
    await service.remove();
    // Wait for the service to reappear and retry
    final newService = await findService(client, wifi, ssid: ssid, timeout: const Duration(seconds: 5));
    if (newService != null) {
      await newService.connect();
    } else {
      print('  Failed to recover service after removal.');
      await client.close();
      return;
    }
  }

  // Wait for the service to reach a connected state (online or ready).
  print('Waiting for connection to complete...');
  final completer = Completer<void>();
  final sub = client.serviceChanged.listen((svc) {
    if (svc.objectPath == service.objectPath || svc.name == ssid) {
      print('  Current state: ${svc.state}');
      if (svc.state == 'online' || svc.state == 'ready') {
        completer.complete();
      } else if (svc.state == 'failure') {
        completer.completeError(ConnmanException(svc.objectPath, svc.error));
      }
    }
  });

  try {
    await completer.future.timeout(const Duration(seconds: 30));
    print('\nConnected successfully!');
  } on TimeoutException {
    print('\nConnection timed out.');
  } on ConnmanException catch (e) {
    print('\nConnection failed: $e');
  } finally {
    await sub.cancel();
  }

  print('\nFinal Service Properties:');
  print('  State:       ${service.state}');
  print('  Type:        ${service.type}');
  print('  Security:    ${service.security}');
  print('  Strength:    ${service.strength}');
  print('  AutoConnect: ${service.autoConnect}');
  print('  Nameservers: ${service.nameservers}');

  await client.close();
  print('\nDone.');
}
