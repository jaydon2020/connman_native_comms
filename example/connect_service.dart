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

  print('Connecting to ${service.name} (${service.objectPath})...');

  StreamSubscription<ConnmanService>? sub;
  try {
    // Check if already connected
    if (service.state == 'online' || service.state == 'ready') {
      print('  -> State: ${service.state} (already connected)');
      print('\nSUCCESS: Connected to $ssid');
      return;
    }

    // Track state until success or failure
    final completer = Completer<void>();

    // Set up listener BEFORE calling connect to avoid race conditions
    sub = client.serviceChanged.listen(
      (svc) {
        if (svc.objectPath == service.objectPath) {
          // Skip redundant idle states if we are already trying to connect
          if (svc.state == 'idle') return;

          print('  -> State: ${svc.state}');
          if (svc.state == 'online' || svc.state == 'ready') {
            if (!completer.isCompleted) completer.complete();
          } else if (svc.state == 'failure') {
            if (!completer.isCompleted) {
              completer.completeError(svc.error.isNotEmpty
                  ? svc.error
                  : 'Connection failed with no error details');
            }
          }
        }
      },
      onError: (e) {
        if (!completer.isCompleted) completer.completeError(e);
      },
      cancelOnError: true,
    );

    // Now attempt connection after listener is ready.
    // service.connect() swallows expected D-Bus errors (InProgress, AlreadyConnected,
    // OperationAborted, Failed, NotConnected) so we don't know whether ConnMan will
    // proceed or has already given up.  We need to re-check state after it returns.
    await service.connect();

    // After service.connect() returns, re-check state: the service state may have
    // already changed during the Connect() call, and we may have missed the event
    // if it arrived before the listener captured a terminal state, or if ConnMan
    // returned an error that service.connect() swallowed without any state change.
    if (!completer.isCompleted) {
      // Re-read the cached service state (updated by serviceChanged events)
      final current = client.services
          .where((s) => s.objectPath == service.objectPath)
          .firstOrNull;
      if (current != null) {
        if (current.state == 'online' || current.state == 'ready') {
          completer.complete();
        } else if (current.state == 'failure') {
          completer.completeError(current.error.isNotEmpty
              ? current.error
              : 'Connection failed with no error details');
        } else if (current.state == 'idle' || current.state == 'disconnect') {
          // ConnMan gave up without transitioning to 'failure' — likely the
          // Connect() D-Bus call returned an error that service.connect() swallowed.
          completer.completeError(
            'Connection attempt ended (service state: ${current.state}). '
            'Try removing the service first: connmanctl remove ${service.objectPath}',
          );
        }
      }
    }

    // Wait for connection to complete with a fixed connection timeout.
    // This is separate from the scan timeout (--timeout flag) because connecting
    // may require agent interaction (password prompt) which needs much more time.
    await completer.future.timeout(kConnectionTimeout);
    print('\nSUCCESS: Connected to $ssid');
  } catch (e) {
    print('\nFAILED: $e');
    print('If it aborted, try removing the service first: connmanctl remove ${service.objectPath}');
  } finally {
    if (sub != null) {
      await sub.cancel();
    }
    await client.close();
  }
}
