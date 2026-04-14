import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';
import 'package:flutter/material.dart';

import 'technology_screen.dart';

/// Home screen — lists all ConnMan technologies (WiFi, Ethernet, …).
/// Tapping a technology navigates to [TechnologyScreen] which shows its
/// networks and allows scanning / connecting.
class ScannerScreen extends StatefulWidget {
  const ScannerScreen({super.key});

  @override
  State<ScannerScreen> createState() => _ScannerScreenState();
}

class _ScannerScreenState extends State<ScannerScreen> {
  final _client = ConnmanClient();

  StreamSubscription<ConnmanTechnology>? _addedSub;
  StreamSubscription<ConnmanTechnology>? _changedSub;
  StreamSubscription<ConnmanTechnology>? _removedSub;

  bool _connected = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _init();
  }

  Future<void> _init() async {
    try {
      await _client.connect();
      if (!mounted) return;
      setState(() => _connected = true);

      // Rebuild whenever the technology list changes (add/remove/power-toggle).
      _addedSub = _client.technologyAdded.listen(_onTechEvent);
      _changedSub = _client.technologyChanged.listen(_onTechEvent);
      _removedSub = _client.technologyRemoved.listen(_onTechEvent);
    } catch (e) {
      if (!mounted) return;
      setState(() => _error = e.toString());
    }
  }

  void _onTechEvent(ConnmanTechnology _) {
    if (mounted) setState(() {});
  }

  @override
  void dispose() {
    _addedSub?.cancel();
    _changedSub?.cancel();
    _removedSub?.cancel();
    _client.close();
    super.dispose();
  }

  // ── Build ─────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    if (_error != null) return _buildError();

    final techs = _client.technologies;

    return Scaffold(
      appBar: AppBar(title: const Text('Network Manager')),
      body: !_connected
          ? const Center(child: CircularProgressIndicator())
          : techs.isEmpty
              ? const Center(
                  child: Text(
                    'No technologies found.',
                    style: TextStyle(color: Colors.grey),
                  ),
                )
              : ListView.separated(
                  itemCount: techs.length,
                  separatorBuilder: (_, __) =>
                      const Divider(height: 1, indent: 72),
                  itemBuilder: (context, index) => _TechnologyTile(
                    technology: techs[index],
                    client: _client,
                  ),
                ),
    );
  }

  Widget _buildError() {
    return Scaffold(
      appBar: AppBar(title: const Text('Network Manager')),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.error_outline, size: 64, color: Colors.red),
              const SizedBox(height: 16),
              Text(
                'Failed to initialise ConnMan',
                style: Theme.of(context).textTheme.titleLarge,
              ),
              const SizedBox(height: 8),
              Text(
                _error!,
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 16),
              const Text(
                'Make sure libconnman_nc.so is built and either '
                'CONNMAN_NC_LIB is set or the library is on the '
                'system library path.',
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      ),
    );
  }
}

// ── Technology tile ───────────────────────────────────────────────────────────

class _TechnologyTile extends StatelessWidget {
  final ConnmanTechnology technology;
  final ConnmanClient client;

  const _TechnologyTile({
    required this.technology,
    required this.client,
  });

  @override
  Widget build(BuildContext context) {
    final tech = technology;
    final statusText = _statusText(tech);

    return ListTile(
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      leading: CircleAvatar(
        backgroundColor: tech.powered
            ? Theme.of(context).colorScheme.primaryContainer
            : Colors.grey.shade200,
        child: Icon(
          _iconFor(tech.type),
          color: tech.powered
              ? Theme.of(context).colorScheme.onPrimaryContainer
              : Colors.grey,
        ),
      ),
      title: Text(
        tech.name.isNotEmpty ? tech.name : tech.type,
        style: const TextStyle(fontWeight: FontWeight.w500),
      ),
      subtitle: Text(
        statusText,
        style: TextStyle(
          color: tech.connected ? Colors.green : null,
        ),
      ),
      trailing: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (tech.connected)
            const Icon(Icons.check_circle, color: Colors.green, size: 18),
          const SizedBox(width: 4),
          const Icon(Icons.chevron_right),
        ],
      ),
      onTap: () => Navigator.push(
        context,
        MaterialPageRoute<void>(
          builder: (_) => TechnologyScreen(client: client, technology: tech),
        ),
      ),
    );
  }

  String _statusText(ConnmanTechnology tech) {
    if (!tech.powered) return 'Off';
    if (tech.connected) return 'Connected';
    return 'On · not connected';
  }

  IconData _iconFor(String type) {
    switch (type) {
      case 'wifi':
        return Icons.wifi;
      case 'ethernet':
        return Icons.settings_ethernet;
      case 'bluetooth':
        return Icons.bluetooth;
      case 'cellular':
        return Icons.signal_cellular_4_bar;
      default:
        return Icons.device_unknown;
    }
  }
}
