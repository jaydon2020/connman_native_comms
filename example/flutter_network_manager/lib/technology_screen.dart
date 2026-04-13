import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';
import 'package:flutter/material.dart';

/// Displays live technology properties and a list of its associated services.
///
/// Analogous to CharacteristicScreen in the BLE scanner — demonstrates the
/// zero-copy Stream<ConnmanTechnology> notification path: state updates arrive
/// directly from the sdbus-cpp event loop thread via Dart_PostCObject_DL.
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

  ConnmanTechnology get _tech => widget.technology;

  @override
  void initState() {
    super.initState();
    // Zero-copy stream: state updates arrive via Dart_PostCObject_DL without
    // any additional copying through the Dart heap.
    _techSub = widget.client.technologyChanged.listen(_onTechChanged);
  }

  void _onTechChanged(ConnmanTechnology tech) {
    if (!mounted || tech.type != _tech.type) return;
    setState(() {});
  }

  @override
  void dispose() {
    _techSub?.cancel();
    super.dispose();
  }

  Future<void> _togglePower() async {
    try {
      await _tech.setPowered(!_tech.powered);
    } on ConnmanException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed: ${e.message}')),
        );
      }
    }
  }

  Future<void> _scan() async {
    try {
      await _tech.scan();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Scan triggered.')),
        );
      }
    } on ConnmanException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Scan failed: ${e.message}')),
        );
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
      appBar: AppBar(title: Text(_tech.name)),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Type: ${_tech.type}',
                style: Theme.of(context).textTheme.bodySmall),
            const SizedBox(height: 8),
            Row(
              children: [
                const Text('Powered:'),
                const SizedBox(width: 8),
                Switch(value: _tech.powered, onChanged: (_) => _togglePower()),
              ],
            ),
            Row(
              children: [
                const Text('Connected:'),
                const SizedBox(width: 8),
                Icon(
                  _tech.connected ? Icons.check_circle : Icons.cancel,
                  color: _tech.connected ? Colors.green : Colors.grey,
                  size: 20,
                ),
              ],
            ),
            if (_tech.tethering) ...[
              Text('Tethering SSID: ${_tech.tetheringIdentifier}'),
              Text('Tethering passphrase: ${_tech.tetheringPassphrase}'),
            ],
            const Divider(height: 32),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text('Services (${services.length})',
                    style: Theme.of(context).textTheme.titleSmall),
                TextButton.icon(
                  icon: const Icon(Icons.refresh, size: 16),
                  label: const Text('Scan'),
                  onPressed: _tech.powered ? _scan : null,
                ),
              ],
            ),
            const SizedBox(height: 8),
            Expanded(
              child: services.isEmpty
                  ? const Center(child: Text('No services.'))
                  : ListView.builder(
                      itemCount: services.length,
                      itemBuilder: (context, index) {
                        final svc = services[index];
                        return ListTile(
                          dense: true,
                          title: Text(
                              svc.name.isNotEmpty ? svc.name : '(hidden)'),
                          subtitle: Text(svc.state),
                          trailing: Text('${svc.strength}'),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
