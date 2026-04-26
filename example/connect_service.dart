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
    print('\n[Agent] AUTHENTICATION REQUIRED for $path');
    stdout.write('Enter WiFi Password: ');
    final pass = stdin.readLineSync() ?? '';
    client.agentSetPassphrase(path, pass);
  });

  client.agentReportError.listen((error) {
    print('\n[Agent] AUTH ERROR for ${error.$1}: ${error.$2}');
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
    await Future<void>.delayed(const Duration(seconds: 2));
  }

  // 1. FORCE RESET: Disconnect and Purge any existing record for this SSID.
  var existing = client.services.where((s) => s.name == ssid).toList();
  if (existing.isNotEmpty) {
    for (final s in existing) {
      print('Resetting stale profile for "${s.name}" (${s.objectPath})...');
      
      try {
        // Disconnect first to break any hardware locks
        await s.disconnect().timeout(const Duration(seconds: 5));
      } catch (_) {}

      final removed = Completer<void>();
      final sub = client.serviceRemoved.listen((rs) {
        if (rs.objectPath == s.objectPath) removed.complete();
      });
      
      try {
        await s.remove();
        // Increased purge wait to 10s for slow Pis
        await removed.future.timeout(const Duration(seconds: 10));
        print('  Profile fully cleared from ConnMan.');
      } catch (e) {
        print('  Warning: profile clear timed out or failed ($e). Continuing...');
      } finally {
        await sub.cancel();
      }
    }
    
    // Crucial: Wait for ConnMan to settle after the purge
    await Future<void>.delayed(const Duration(seconds: 2));
  }

  // 2. FRESH SCAN: Force a scan to find the "new" service object.
  print('Requesting fresh WiFi scan...');
  await wifi.scan();
  
  // Wait a moment for scan results to propagate to services list
  await Future<void>.delayed(const Duration(seconds: 2));

  print('Searching for "$ssid"...');
  ConnmanService? service = client.services
      .where((s) => s.name == ssid && s.type == 'wifi')
      .firstOrNull;

  if (service == null) {
    print('Service "$ssid" still not visible. Waiting for signal...');
    try {
      service = await client.serviceAdded
          .where((s) => s.name == ssid && s.type == 'wifi')
          .first
          .timeout(timeout);
    } on TimeoutException {
      print('ERROR: Service "$ssid" not found after reset and scan.');
      await client.close();
      return;
    }
  }

  print('Connecting to "${service.name}" (${service.objectPath})...');

  try {
    // 3. Initiate connection (native timeout is 60s).
    await service.connect();
    
    // 4. Wait for completion signals.
    print('Waiting for D-Bus handshake (up to 60 seconds)...');
    final completer = Completer<void>();
    final sub = client.serviceChanged.listen((svc) {
      if (svc.objectPath == service!.objectPath) {
        print('  Status Update: ${svc.state} ${svc.error.isNotEmpty ? "(Error: ${svc.error})" : ""}');
        if (svc.state == 'online' || svc.state == 'ready') {
          completer.complete();
        } else if (svc.state == 'failure') {
          completer.completeError(ConnmanException(svc.objectPath, svc.error));
        }
      }
    });

    try {
      await completer.future.timeout(const Duration(seconds: 65));
      print('\nSUCCESS: WiFi connected!');
    } finally {
      await sub.cancel();
    }
  } on ConnmanOperationAbortedException {
    print('\nERROR: Connection aborted. This happens if the password is wrong or another network manager (like wpa_supplicant directly) is fighting ConnMan.');
  } on TimeoutException {
    print('\nERROR: Handshake timed out.');
  } on ConnmanException catch (e) {
    print('\nERROR: Connection failed: $e');
  }

  await client.close();
  print('\nDone.');
}
