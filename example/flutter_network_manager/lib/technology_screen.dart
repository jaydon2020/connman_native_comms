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
  StreamSubscription<String>? _agentSub;

  bool _scanning = false;
  // Track services that are currently connecting or disconnecting
  final Set<String> _connectingServicePaths = {};

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
      // Clear connecting state when service reaches a terminal state
      const terminalStates = {'online', 'ready', 'idle', 'failure', 'disconnect'};
      if (terminalStates.contains(svc.state)) {
        _connectingServicePaths.remove(svc.objectPath);
      }
      setState(() {});
    });
    _svcRemovedSub = widget.client.serviceRemoved.listen((svc) {
      if (!mounted) return;
      setState(() {});
    });

    _agentSub = widget.client.agentRequestInput.listen(_onAgentRequestInput);
  }

  void _onAgentRequestInput(String path) {
    if (!mounted) return;
    // Only handle if it's a service belonging to this technology
    final svc = widget.client.services
        .where((s) => s.objectPath == path && s.type == _tech.type)
        .firstOrNull;
    if (svc != null) {
      _showPassphraseDialog(svc);
    }
  }

  Future<void> _showPassphraseDialog(ConnmanService svc) async {
    final controller = TextEditingController();
    final passphrase = await showDialog<String>(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        title: const Text('Passphrase Required'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text('Enter passphrase for ${svc.name}:'),
            TextField(
              controller: controller,
              obscureText: true,
              autofocus: true,
              decoration: const InputDecoration(hintText: 'Passphrase'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, controller.text),
            child: const Text('Connect'),
          ),
        ],
      ),
    );

    if (!mounted) return;

    if (passphrase == null) {
      widget.client.agentClearPassphrase(svc.objectPath);
      _connectingServicePaths.remove(svc.objectPath);
      setState(() {});
    } else {
      widget.client.agentSetPassphrase(svc.objectPath, passphrase);
    }
  }

  @override
  void dispose() {
    _techSub?.cancel();
    _svcAddedSub?.cancel();
    _svcChangedSub?.cancel();
    _svcRemovedSub?.cancel();
    _agentSub?.cancel();
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

  Future<void> _connectService(ConnmanService svc) async {
    final path = svc.objectPath;
    if (_connectingServicePaths.contains(path)) return;

    setState(() => _connectingServicePaths.add(path));
    try {
      await svc.connect();
    } on ConnmanException catch (e) {
      if (mounted) {
        _connectingServicePaths.remove(path);
        setState(() {});
        ScaffoldMessenger.of(context)
          ..clearSnackBars()
          ..showSnackBar(SnackBar(content: Text('Connect failed: ${e.message}')));
      }
    }
  }

  Future<void> _disconnectService(ConnmanService svc) async {
    final path = svc.objectPath;
    if (_connectingServicePaths.contains(path)) return;

    setState(() => _connectingServicePaths.add(path));
    try {
      await svc.disconnect();
    } on ConnmanException catch (e) {
      if (mounted) {
        _connectingServicePaths.remove(path);
        setState(() {});
        ScaffoldMessenger.of(context)
          ..clearSnackBars()
          ..showSnackBar(SnackBar(content: Text('Disconnect failed: ${e.message}')));
      }
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
                      final isConnecting =
                          _connectingServicePaths.contains(svc.objectPath);

                      return ListTile(
                        dense: true,
                        leading: _strengthIcon(svc.strength),
                        title: Text(name),
                        subtitle: Text(svc.state),
                        trailing: _buildServiceTrailing(svc, isConnected, isConnecting),
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

  Widget _buildServiceTrailing(
    ConnmanService svc,
    bool isConnected,
    bool isConnecting,
  ) {
    if (isConnecting) {
      return const SizedBox(
        width: 24,
        height: 24,
        child: CircularProgressIndicator(strokeWidth: 2),
      );
    }

    if (isConnected) {
      return Row(
        mainAxisSize: MainAxisSize.min,
        mainAxisAlignment: MainAxisAlignment.end,
        children: [
          const Icon(Icons.check_circle, color: Colors.green, size: 18),
          const SizedBox(width: 8),
          FilledButton.tonal(
            onPressed: () => _disconnectService(svc),
            style: FilledButton.styleFrom(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
              backgroundColor: Colors.red.shade100,
              foregroundColor: Colors.red,
            ),
            child: const Text('Disconnect', style: TextStyle(fontSize: 11)),
          ),
        ],
      );
    }

    return Row(
      mainAxisSize: MainAxisSize.min,
      mainAxisAlignment: MainAxisAlignment.end,
      children: [
        Text(
          '${svc.strength}',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(width: 8),
        FilledButton.tonal(
          onPressed: () => _connectService(svc),
          style: FilledButton.styleFrom(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
          ),
          child: const Text('Connect', style: TextStyle(fontSize: 11)),
        ),
      ],
    );
  }
}
