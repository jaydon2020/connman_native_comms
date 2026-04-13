import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';
import 'package:flutter/material.dart';

import 'technology_screen.dart';

class ServiceScreen extends StatefulWidget {
  final ConnmanClient client;
  final ConnmanService service;

  const ServiceScreen({
    super.key,
    required this.client,
    required this.service,
  });

  @override
  State<ServiceScreen> createState() => _ServiceScreenState();
}

class _ServiceScreenState extends State<ServiceScreen> {
  bool _connecting = false;
  StreamSubscription<ConnmanService>? _changedSub;
  StreamSubscription<ConnmanTechnology>? _techSub;

  ConnmanService get _svc => widget.service;

  @override
  void initState() {
    super.initState();
    // Keep the UI in sync with live state updates.
    _changedSub = widget.client.serviceChanged.listen(_onServiceChanged);
    // Pop back if WiFi is powered off.
    _techSub = widget.client.technologyChanged.listen(_onTechChanged);
  }

  void _onServiceChanged(ConnmanService svc) {
    if (!mounted) return;
    if (svc.objectPath != _svc.objectPath) return;
    if (svc.state == 'idle' && _svc.state != 'idle') {
      // Service disconnected — pop back to scanner.
      Navigator.of(context).popUntil((route) => route.isFirst);
      return;
    }
    setState(() {});
  }

  void _onTechChanged(ConnmanTechnology tech) {
    if (!mounted) return;
    if (tech.type == 'wifi' && !tech.powered) {
      // Technology powered off — pop back to scanner.
      Navigator.of(context).popUntil((route) => route.isFirst);
      return;
    }
  }

  Future<void> _connect() async {
    setState(() => _connecting = true);
    try {
      await _svc.connect();
    } on ConnmanException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Connect failed: ${e.message}')),
        );
      }
    }
    if (mounted) setState(() => _connecting = false);
  }

  Future<void> _disconnect() async {
    await _svc.disconnect();
    // _onServiceChanged will pop back when state becomes idle.
  }

  @override
  void dispose() {
    _changedSub?.cancel();
    _techSub?.cancel();
    super.dispose();
  }

  bool get _isConnected =>
      _svc.state == 'online' || _svc.state == 'ready';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(_svc.name.isNotEmpty ? _svc.name : '(hidden)'),
        actions: [
          if (_connecting)
            const Padding(
              padding: EdgeInsets.all(16),
              child: SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(strokeWidth: 2),
              ),
            )
          else
            TextButton(
              onPressed: _isConnected ? _disconnect : _connect,
              child: Text(_isConnected ? 'Disconnect' : 'Connect'),
            ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _PropertyTile('State', _svc.state),
          _PropertyTile('Type', _svc.type),
          _PropertyTile('Strength', '${_svc.strength}'),
          _PropertyTile('Security', _svc.security.join(', ')),
          _PropertyTile('Favorite', '${_svc.favorite}'),
          _PropertyTile('AutoConnect', '${_svc.autoConnect}'),
          _PropertyTile('Roaming', '${_svc.roaming}'),
          _PropertyTile('Nameservers', _svc.nameservers.join(', ')),
          _PropertyTile('Domains', _svc.domains.join(', ')),
          const Divider(height: 32),
          ListTile(
            leading: const Icon(Icons.settings_ethernet),
            title: const Text('Technology Details'),
            trailing: const Icon(Icons.chevron_right),
            onTap: () {
              final wifi = widget.client.technologies
                  .where((t) => t.type == 'wifi')
                  .firstOrNull;
              if (wifi != null) {
                Navigator.push(
                  context,
                  MaterialPageRoute<void>(
                    builder: (_) => TechnologyScreen(
                        client: widget.client, technology: wifi),
                  ),
                );
              }
            },
          ),
        ],
      ),
    );
  }
}

class _PropertyTile extends StatelessWidget {
  final String label;
  final String value;

  const _PropertyTile(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 120,
            child: Text(
              label,
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
          Expanded(child: Text(value)),
        ],
      ),
    );
  }
}
