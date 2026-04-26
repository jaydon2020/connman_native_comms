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
    await Future<void>.delayed(const Duration(seconds: 1));
  }

  // 1. FORCED PURGE: Remove any existing record for this SSID to clear bad passwords.
  var existing = client.services.where((s) => s.name == ssid).toList();
  if (existing.isNotEmpty) {
    for (final s in existing) {
      print('Purging stale profile for "${s.name}" (${s.objectPath})...');
      
      final removed = Completer<void>();
      final sub = client.serviceRemoved.listen((rs) {
        if (rs.objectPath == s.objectPath) removed.complete();
      });
      
      try {
        await s.remove();
        await removed.future.timeout(const Duration(seconds: 5));
        print('  Profile cleared.');
      } catch (e) {
        print('  Warning: profile clear timed out (continuing anyway).');
      } finally {
        await sub.cancel();
      }
    }
    
    // Give ConnMan a moment to settle
    await Future<void>.delayed(const Duration(seconds: 1));
  }

  // 2. FRESH SCAN: Find the service object after the purge.
  print('Scanning for fresh service object...');
  await wifi.scan();
  
  ConnmanService? service;
  try {
    service = await client.serviceAdded
        .where((s) => s.name == ssid && s.type == 'wifi')
        .first
        .timeout(timeout);
  } on TimeoutException {
    // Check if it's already there but we missed the signal
    service = client.services
      .where((s) => s.name == ssid && s.type == 'wifi')
      .firstOrNull;
  }

  if (service == null) {
    print('Service "$ssid" not found after purge and scan.');
    await client.close();
    return;
  }

  print('Connecting to "${service.name}" (${service.objectPath})...');

  try {
    // 3. Initiate connection (native timeout is now 60s).
    await service.connect();
    
    // 4. Wait for completion signals.
    print('Waiting for D-Bus signals (this may take up to 60 seconds)...');
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
      print('\nSUCCESS: Connected successfully!');
    } finally {
      await sub.cancel();
    }
  } on ConnmanOperationAbortedException {
    print('\nERROR: Connection aborted. Check if the password is correct or if another manager is fighting for WiFi.');
  } on TimeoutException {
    print('\nERROR: Connection timed out.');
  } on ConnmanException catch (e) {
    print('\nERROR: Connection failed: $e');
  }

  await client.close();
  print('\nDone.');
}
