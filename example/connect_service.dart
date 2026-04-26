// example/connect_service.dart — connect to a WiFi network by SSID.
import 'dart:async';
import 'dart:io';
import 'package:connman_native_comms/connman_native_comms.dart';

import 'example_utils.dart';

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    print('Usage: dart run example/connect_service.dart <ssid> [--timeout <seconds>]');
    return;
  }

  final ssid = args[0];
  final timeout = parseScanTimeout(args);
  final client = ConnmanClient();
  
  // Initialize Agent listeners
  client.agentRequestInput.listen((path) {
    print('\n[Agent] Password required for $path');
    stdout.write('Enter WiFi Password: ');
    final pass = stdin.readLineSync() ?? '';
    client.agentSetPassphrase(path, pass);
  });

  client.agentReportError.listen((error) {
    print('\n[Agent] Authentication Error: ${error.$2}');
  });

  await client.connect();

  final wifi = client.technologies.where((t) => t.type == 'wifi').firstOrNull;
  if (wifi == null || !wifi.powered) {
    print('WiFi is missing or powered off.');
    await client.close();
    return;
  }

  // Find the service
  print('Searching for "$ssid"...');
  var service = await findService(client, wifi, ssid: ssid, timeout: timeout);
  if (service == null) {
    await client.close();
    return;
  }

  print('Connecting to ${service.name} (${service.objectPath})...');

  try {
    // Attempt connection
    await service.connect();
    
    // Track state until success or failure
    final completer = Completer<void>();
    final sub = client.serviceChanged.listen((svc) {
      if (svc.objectPath == service!.objectPath) {
        print('  -> State: ${svc.state}');
        if (svc.state == 'online' || svc.state == 'ready') completer.complete();
        else if (svc.state == 'failure') completer.completeError(svc.error);
      }
    });

    await completer.future.timeout(const Duration(seconds: 60));
    print('\nSUCCESS: Connected to $ssid');
  } catch (e) {
    print('\nFAILED: $e');
    print('If it aborted, try removing the service first: connmanctl remove ${service.objectPath}');
  } finally {
    await client.close();
  }
}
