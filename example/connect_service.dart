// example/connect_service.dart — connect to a WiFi network by SSID.
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:connman_native_comms/connman_native_comms.dart';

import 'example_utils.dart';

/// Default connection timeout — intentionally longer than scan timeout because
/// the D-Bus Connect() call may block while the agent waits for user input.
const Duration kConnectionTimeout = Duration(seconds: 60);

Future<void> main(List<String> args) async {
  if (args.isEmpty) {
    print('Usage: dart run example/connect_service.dart <ssid> [--timeout <seconds>]');
    return;
  }

  final ssid = args[0];
  final scanTimeout = parseScanTimeout(args);
  final client = ConnmanClient();

  // Initialize Agent listeners
  client.agentRequestInput.listen((path) async {
    print('\n[Agent] Password required for $path');
    stdout.write('Enter WiFi Password: ');

    String pass = '';
    final bool originalEchoMode = stdin.echoMode;
    final bool originalLineMode = stdin.lineMode;

    try {
      // Disable echo for password entry
      if (stdin.hasTerminal) {
        stdin.echoMode = false;
        stdin.lineMode = false;
      }

      // Read password character by character to remain non-blocking to the isolate
      final List<int> codes = [];
      await for (final List<int> chunk in stdin) {
        bool breakAfterChunk = false;
        for (final int code in chunk) {
          if (code == 10 || code == 13) {
            // Newline/Enter
            breakAfterChunk = true;
            break;
          } else if (code == 127 || code == 8) {
            // Backspace
            if (codes.isNotEmpty) codes.removeLast();
          } else {
            codes.add(code);
          }
        }
        if (breakAfterChunk) break;
      }
      pass = utf8.decode(codes);
    } finally {
      if (stdin.hasTerminal) {
        stdin.echoMode = originalEchoMode;
        stdin.lineMode = originalLineMode;
      }
      stdout.writeln(); // Move to next line after Enter
    }

    client.agentSetPassphrase(path, pass);
  });

  client.agentReportError.listen((error) {
    print('\n[Agent] Authentication Error: ${error.$2}');
    print('[Agent] ConnMan will re-prompt for credentials if retrying.');
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
  final service = await findService(client, wifi, ssid: ssid, timeout: scanTimeout);
  if (service == null) {
    await client.close();
    return;
  }

  // Check if already connected
  if (service.state == 'online' || service.state == 'ready') {
    print('  -> State: ${service.state} (already connected)');
    print('\nSUCCESS: Connected to $ssid');
    await client.close();
    return;
  }

  print('Connecting to ${service.name} (${service.objectPath})...');

  // First attempt
  var result = await _tryConnect(client, service);

  // If the connection returned to idle it means ConnMan had stale cached
  // credentials and failed without calling the agent.  Remove the service
  // config to clear cached creds, re-scan to discover a fresh entry, then
  // retry — ConnMan will call the agent this time.
  if (result == _ConnectResult.staleIdle) {
    print('  Service has stale credentials — removing and retrying...');

    final removedPath = service.objectPath;

    try {
      await service.remove();
    } catch (e) {
      print('\nFAILED: Could not remove stale service: $e');
      await client.close();
      return;
    }

    // Wait for ConnMan to actually process the removal — the Dart cache
    // won't be updated until the kServiceRemoved event arrives.
    try {
      await client.serviceRemoved
          .where((s) => s.objectPath == removedPath)
          .first
          .timeout(const Duration(seconds: 5));
    } on TimeoutException {
      // If the event didn't arrive, the service might have already been
      // re-added by a background scan.  Continue anyway.
    }

    // Trigger a fresh scan and wait for the service to re-appear.
    print('Scanning for "$ssid"...');
    await wifi.scan();
    ConnmanService? freshService;
    try {
      freshService = await client.serviceAdded
          .where((s) => s.name == ssid && s.type == 'wifi')
          .first
          .timeout(scanTimeout);
    } on TimeoutException {
      // Maybe it was re-added during scan before we started listening.
      // Check the cache.
      freshService = client.services
          .where((s) => s.name == ssid && s.type == 'wifi')
          .firstOrNull;
    }

    if (freshService == null) {
      print('\nFAILED: Service "$ssid" not found after removal.');
      await client.close();
      return;
    }

    print('Connecting to ${freshService.name} (${freshService.objectPath})...');
    result = await _tryConnect(client, freshService);
  }

  switch (result) {
    case _ConnectResult.success:
      print('\nSUCCESS: Connected to $ssid');
    case _ConnectResult.staleIdle:
      print('\nFAILED: Connection ended with idle state.');
      print('Try manually: connmanctl remove ${service.objectPath}');
    case _ConnectResult.failure:
      // Error details already printed by _tryConnect
      break;
  }

  await client.close();
}

// ── Connection helper ──────────────────────────────────────────────────────

enum _ConnectResult { success, failure, staleIdle }

/// Attempt to connect to [service] and wait for a terminal state.
///
/// Returns the outcome so the caller can decide whether to retry.
Future<_ConnectResult> _tryConnect(
    ConnmanClient client, ConnmanService service) async {
  final completer = Completer<_ConnectResult>();
  StreamSubscription<ConnmanService>? sub;

  try {
    // Set up listener BEFORE calling connect to avoid race conditions
    sub = client.serviceChanged.listen(
      (svc) {
        if (svc.objectPath == service.objectPath) {
          if (svc.state == 'idle') return; // skip transient idle

          print('  -> State: ${svc.state}');
          if (svc.state == 'online' || svc.state == 'ready') {
            if (!completer.isCompleted) {
              completer.complete(_ConnectResult.success);
            }
          } else if (svc.state == 'failure') {
            if (!completer.isCompleted) {
              final reason = svc.error.isNotEmpty
                  ? svc.error
                  : 'Connection failed with no error details';
              print('  [error] $reason');
              completer.complete(_ConnectResult.failure);
            }
          }
        }
      },
      onError: (e) {
        if (!completer.isCompleted) {
          print('  [error] $e');
          completer.complete(_ConnectResult.failure);
        }
      },
    );

    // Call serviceConnect() directly instead of service.connect() so we can
    // see the actual D-Bus error that ConnMan returns.
    try {
      await client.serviceConnect(service.objectPath);
    } on ConnmanInProgressException {
      print('  [info] Already connecting (InProgress)');
    } on ConnmanAlreadyConnectedException {
      if (!completer.isCompleted) completer.complete(_ConnectResult.success);
    } catch (e) {
      // Log the real error so we know what ConnMan returned
      print('  [D-Bus error] $e');
    }

    // After serviceConnect() returns, re-check state: ConnMan may have
    // returned an error without any state change.
    if (!completer.isCompleted) {
      final current = client.services
          .where((s) => s.objectPath == service.objectPath)
          .firstOrNull;
      if (current != null) {
        if (current.state == 'online' || current.state == 'ready') {
          completer.complete(_ConnectResult.success);
        } else if (current.state == 'failure') {
          final reason = current.error.isNotEmpty
              ? current.error
              : 'Connection failed with no error details';
          print('  [error] $reason');
          completer.complete(_ConnectResult.failure);
        } else if (current.state == 'idle' || current.state == 'disconnect') {
          completer.complete(_ConnectResult.staleIdle);
        }
      }
    }

    // Wait with a generous timeout (agent interaction may need user input)
    return await completer.future.timeout(kConnectionTimeout);
  } on TimeoutException {
    print('\nFAILED: Connection timed out after ${kConnectionTimeout.inSeconds}s');
    return _ConnectResult.failure;
  } finally {
    await sub?.cancel();
  }
}

