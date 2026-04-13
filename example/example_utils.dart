// example/example_utils.dart — shared helpers for CLI examples.

import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';

/// Default scan timeout in seconds.
const int kDefaultTimeoutSeconds = 15;

/// Parse `--timeout <seconds>` from [args], returning the Duration.
Duration parseScanTimeout(List<String> args) {
  final idx = args.indexOf('--timeout');
  if (idx != -1 && idx + 1 < args.length) {
    final seconds = int.tryParse(args[idx + 1]);
    if (seconds != null && seconds > 0) return Duration(seconds: seconds);
  }
  return const Duration(seconds: kDefaultTimeoutSeconds);
}

/// Find a WiFi service by [ssid], checking known services first then scanning.
///
/// Returns `null` if the service is not found within [timeout].
Future<ConnmanService?> findService(
  ConnmanClient client,
  ConnmanTechnology wifiTech, {
  required String ssid,
  required Duration timeout,
}) async {
  // Check already-known services.
  final existing = client.services
      .where((s) => s.name == ssid && s.type == 'wifi')
      .firstOrNull;
  if (existing != null) {
    print('Service "$ssid" already known to ConnMan.');
    return existing;
  }

  // Fall back to scanning.
  print('Scanning for "$ssid"...');
  await wifiTech.scan();

  try {
    final service = await client.serviceAdded
        .where((s) => s.name == ssid && s.type == 'wifi')
        .first
        .timeout(timeout);
    return service;
  } on TimeoutException {
    print('Service "$ssid" not found within ${timeout.inSeconds} seconds.');
    return null;
  }
}
