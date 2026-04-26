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

  // 1. Clear any stale state for this SSID.
  final existing = client.services.where((s) => s.name == ssid).toList();
  for (final s in existing) {
    print('Removing stale service record for "${s.name}" (${s.objectPath})...');
    final removed = Completer<void>();
    final sub = client.serviceRemoved.listen((rs) {
      if (rs.objectPath == s.objectPath) removed.complete();
    });
    try {
      await s.remove();
      await removed.future.timeout(const Duration(seconds: 2));
    } catch (_) {} finally {
      await sub.cancel();
    }
  }

  // 2. Scan and find the fresh service object.
  final service = await findService(client, wifi, ssid: ssid, timeout: timeout);
  if (service == null) {
    print('Service "$ssid" not found after scan.');
    await client.close();
    return;
  }

  print('Connecting to "${service.name}" (${service.objectPath})...');

  try {
    // 3. Initiate connection.
    await service.connect();
    
    // 4. Wait for completion signals.
    print('Waiting for connection to complete...');
    final completer = Completer<void>();
    final sub = client.serviceChanged.listen((svc) {
      if (svc.objectPath == service.objectPath) {
        print('  Current state: ${svc.state}');
        if (svc.state == 'online' || svc.state == 'ready') {
          completer.complete();
        } else if (svc.state == 'failure') {
          completer.completeError(ConnmanException(svc.objectPath, svc.error));
        }
      }
    });

    try {
      await completer.future.timeout(const Duration(seconds: 45));
      print('\nConnected successfully!');
    } finally {
      await sub.cancel();
    }
  } on ConnmanOperationAbortedException {
    print('\nConnection aborted by ConnMan. This usually means the agent was not ready.');
  } on TimeoutException {
    print('\nConnection timed out.');
  } on ConnmanException catch (e) {
    print('\nConnection failed: $e');
  }

  print('\nFinal Service Properties:');
  print('  State:       ${service.state}');
  print('  Type:        ${service.type}');
  print('  Security:    ${service.security}');
  print('  Strength:    ${service.strength}');

  await client.close();
  print('\nDone.');
}
