import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';
import 'package:flutter/material.dart';

import 'service_screen.dart';

/// Displays live technology properties and the list of associated services.
///
/// Subscribes to technologyChanged for power/connected state and to
/// serviceAdded/serviceChanged/serviceRemoved so the services list stays in
/// sync with scan results — previously it only rebuilt on technologyChanged,
/// meaning scan results never appeared.
class TechnologyScreen extends StatefulWidget {
  final ConnmanClient client;
  final ConnmanTechnology technology;

  const TechnologyScreen({
    super.key,
    required this.client,
    required this.technology,
  });

  @override
  State<TechnologyScreen> createState() => _TechnologyScreenState();
}

class _TechnologyScreenState extends State<TechnologyScreen> {
  StreamSubscription<ConnmanTechnology>? _techSub;
  StreamSubscription<ConnmanService>? _svcAddedSub;
  StreamSubscription<ConnmanService>? _svcChangedSub;
  StreamSubscription<ConnmanService>? _svcRemovedSub;

  bool _scanning = false;

  ConnmanTechnology get _tech => widget.technology;

  @override
  void initState() {
    super.initState();

    // Technology property changes (Powered, Connected, Tethering, …).
    _techSub = widget.client.technologyChanged.listen((tech) {
      if (!mounted || tech.type != _tech.type) return;
      // If the technology was powered off, go back to the technology list.
      if (!tech.powered) {
        Navigator.of(context).pop();
        return;
      }
      setState(() {});
    });

    // Service list changes — the bug fix: without these the services card
    // never updates after a scan because only technologyChanged was wired.
    _svcAddedSub = widget.client.serviceAdded.listen((svc) {
      if (!mounted || svc.type != _tech.type) return;
      setState(() {});
    });
    _svcChangedSub = widget.client.serviceChanged.listen((svc) {
      if (!mounted || svc.type != _tech.type) return;
      setState(() {});
    });
    _svcRemovedSub = widget.client.serviceRemoved.listen((svc) {
      if (!mounted) return;
      setState(() {});
    });
  }

  @override
  void dispose() {
    _techSub?.cancel();
    _svcAddedSub?.cancel();
    _svcChangedSub?.cancel();
    _svcRemovedSub?.cancel();
    super.dispose();
  }

  Future<void> _togglePower() async {
    try {
      await _tech.setPowered(!_tech.powered);
    } on ConnmanAlreadyEnabledException {
      // Already on — no-op.
    } on ConnmanAlreadyDisabledException {
      // Already off — no-op.
    } on ConnmanException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context)
          ..clearSnackBars()
          ..showSnackBar(SnackBar(content: Text('Failed: ${e.message}')));
      }
    }
  }

  Future<void> _scan() async {
    if (!_tech.powered || _scanning) return;
    setState(() => _scanning = true);
    try {
      await _tech.scan();
      if (mounted) {
        ScaffoldMessenger.of(context)
          ..clearSnackBars()
          ..showSnackBar(const SnackBar(content: Text('Scan completed.')));
      }
    } on ConnmanException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context)
          ..clearSnackBars()
          ..showSnackBar(SnackBar(content: Text('Scan failed: ${e.message}')));
      }
    } finally {
      if (mounted) setState(() => _scanning = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final services = widget.client.services
        .where((s) => s.type == _tech.type)
        .toList()
      ..sort((a, b) => b.strength.compareTo(a.strength));

    return Scaffold(
      appBar: AppBar(
        title: Text(_tech.name),
        bottom: _scanning
            ? const PreferredSize(
                preferredSize: Size.fromHeight(4),
                child: LinearProgressIndicator(),
              )
            : null,
      ),
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // ── Technology properties card ───────────────────────────────────
          Card(
            margin: const EdgeInsets.all(12),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Type: ${_tech.type}',
                      style: Theme.of(context).textTheme.bodySmall),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      const Text('Powered'),
                      const Spacer(),
                      Switch(
                        value: _tech.powered,
                        onChanged: (_) => _togglePower(),
                      ),
                    ],
                  ),
                  Row(
                    children: [
                      const Text('Connected'),
                      const Spacer(),
                      Icon(
                        _tech.connected ? Icons.check_circle : Icons.cancel,
                        color: _tech.connected ? Colors.green : Colors.grey,
                        size: 20,
                      ),
                    ],
                  ),
                  if (_tech.tethering) ...[
                    const Divider(height: 20),
                    Text('Tethering SSID: ${_tech.tetheringIdentifier}'),
                    Text('Passphrase: ${_tech.tetheringPassphrase}'),
                  ],
                ],
              ),
            ),
          ),

          // ── Services list header ─────────────────────────────────────────
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: Row(
              children: [
                Text(
                  'Networks (${services.length})',
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                const Spacer(),
                TextButton.icon(
                  icon: _scanning
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.refresh, size: 16),
                  label: Text(_scanning ? 'Scanning…' : 'Scan'),
                  onPressed: _tech.powered && !_scanning ? _scan : null,
                ),
              ],
            ),
          ),

          // ── Services list ────────────────────────────────────────────────
          Expanded(
            child: services.isEmpty
                ? Center(
                    child: Text(
                      _tech.powered
                          ? 'No networks found. Tap Scan.'
                          : '${_tech.name.isNotEmpty ? _tech.name : _tech.type} is powered off.',
                      style: const TextStyle(color: Colors.grey),
                    ),
                  )
                : ListView.builder(
                    itemCount: services.length,
                    itemBuilder: (context, index) {
                      final svc = services[index];
                      final name = svc.name.isNotEmpty ? svc.name : '(hidden)';
                      final isConnected =
                          svc.state == 'online' || svc.state == 'ready';

                      return ListTile(
                        dense: true,
                        leading: _strengthIcon(svc.strength),
                        title: Text(name),
                        subtitle: Text(svc.state),
                        trailing: isConnected
                            ? const Icon(Icons.check_circle,
                                color: Colors.green, size: 18)
                            : Text(
                                '${svc.strength}',
                                style: Theme.of(context).textTheme.bodySmall,
                              ),
                        onTap: () => Navigator.push(
                          context,
                          MaterialPageRoute<void>(
                            builder: (_) => ServiceScreen(
                              client: widget.client,
                              service: svc,
                            ),
                          ),
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }

  Widget _strengthIcon(int strength) {
    if (strength >= 70) return const Icon(Icons.signal_wifi_4_bar, size: 20);
    if (strength >= 40) return const Icon(Icons.network_wifi_3_bar, size: 20);
    return const Icon(Icons.network_wifi_1_bar, size: 20);
  }
}
