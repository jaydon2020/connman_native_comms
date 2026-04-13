import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';
import 'package:flutter/material.dart';

import 'service_screen.dart';

class ScannerScreen extends StatefulWidget {
  const ScannerScreen({super.key});

  @override
  State<ScannerScreen> createState() => _ScannerScreenState();
}

class _ScannerScreenState extends State<ScannerScreen> {
  final _client = ConnmanClient();
  final _services = <String, ConnmanService>{};
  StreamSubscription<ConnmanService>? _addedSub;
  StreamSubscription<ConnmanService>? _changedSub;
  StreamSubscription<ConnmanService>? _removedSub;
  StreamSubscription<ConnmanTechnology>? _techSub;
  bool _scanning = false;
  bool _connected = false;
  String? _error;

  ConnmanTechnology? get _wifi {
    final wifis = _client.technologies.where((t) => t.type == 'wifi');
    return wifis.isNotEmpty ? wifis.first : null;
  }

  bool get _powered => _wifi?.powered ?? false;

  @override
  void initState() {
    super.initState();
    _init();
  }

  Future<void> _init() async {
    try {
      await _client.connect();
      if (!mounted) return;
      setState(() {
        _connected = true;
        // Seed with services already known to ConnMan from previous scans.
        for (final svc in _client.services) {
          if (svc.type == 'wifi') _services[svc.objectPath] = svc;
        }
      });
      _addedSub = _client.serviceAdded.listen((svc) {
        if (mounted && svc.type == 'wifi') {
          setState(() => _services[svc.objectPath] = svc);
        }
      });
      _changedSub = _client.serviceChanged.listen((svc) {
        if (mounted && svc.type == 'wifi') {
          setState(() => _services[svc.objectPath] = svc);
        }
      });
      _removedSub = _client.serviceRemoved.listen((svc) {
        if (mounted) setState(() => _services.remove(svc.objectPath));
      });
      // Monitor technology power state changes.
      _techSub = _client.technologyChanged.listen((tech) {
        if (!mounted || tech.type != 'wifi') return;
        setState(() {
          if (tech.powered) {
            // Technology just powered on — seed with known services.
            for (final svc in _client.services) {
              if (svc.type == 'wifi') _services[svc.objectPath] = svc;
            }
          } else {
            if (_scanning) _scanning = false;
            _services.clear();
          }
        });
      });
    } catch (e) {
      if (!mounted) return;
      setState(() => _error = e.toString());
    }
  }

  Future<void> _toggleScan() async {
    final wifi = _wifi;
    if (!_connected || wifi == null) return;

    if (!_powered) {
      await wifi.setPowered(true);
      // Give ConnMan a moment to bring the technology up.
      await Future<void>.delayed(const Duration(milliseconds: 500));
      if (!mounted) return;
      setState(() {});
    }

    if (_scanning) {
      // ConnMan manages scan lifecycle automatically.
    } else {
      _services.clear();
      await wifi.scan();
    }
    if (mounted) setState(() => _scanning = !_scanning);
  }

  @override
  void dispose() {
    _addedSub?.cancel();
    _changedSub?.cancel();
    _removedSub?.cancel();
    _techSub?.cancel();
    _client.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_error != null) {
      return Scaffold(
        appBar: AppBar(title: const Text('Network Manager')),
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(32),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.error_outline, size: 48, color: Colors.red),
                const SizedBox(height: 16),
                Text(
                  'Failed to initialize ConnMan',
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

    final sorted = _services.values.toList()
      ..sort((a, b) => b.strength.compareTo(a.strength));

    return Scaffold(
      appBar: AppBar(
        title: const Text('Network Manager'),
        actions: [
          if (_connected)
            Icon(
              _powered ? Icons.wifi : Icons.wifi_off,
              color: _powered ? null : Colors.red,
            ),
          const SizedBox(width: 8),
          IconButton(
            icon: Icon(_scanning ? Icons.stop : Icons.wifi_find),
            onPressed: _connected ? _toggleScan : null,
          ),
        ],
      ),
      body: !_powered && _connected
          ? Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const Icon(Icons.wifi_off, size: 48, color: Colors.grey),
                  const SizedBox(height: 16),
                  const Text('WiFi is powered off.'),
                  const SizedBox(height: 16),
                  ElevatedButton(
                    onPressed: () => _wifi?.setPowered(true),
                    child: const Text('Power On'),
                  ),
                ],
              ),
            )
          : sorted.isEmpty
              ? const Center(
                  child:
                      Text('No networks found. Tap scan to start.'))
              : ListView.builder(
                  itemCount: sorted.length,
                  itemBuilder: (context, index) {
                    final svc = sorted[index];
                    final name =
                        svc.name.isNotEmpty ? svc.name : '(hidden)';
                    return ListTile(
                      leading: _strengthIcon(svc.strength),
                      title: Text(name),
                      subtitle: Text(svc.state),
                      trailing: svc.state == 'online' || svc.state == 'ready'
                          ? const Icon(Icons.check_circle,
                              color: Colors.green)
                          : null,
                      onTap: () => Navigator.push(
                        context,
                        MaterialPageRoute<void>(
                          builder: (_) => ServiceScreen(
                              client: _client, service: svc),
                        ),
                      ),
                    );
                  },
                ),
    );
  }

  Widget _strengthIcon(int strength) {
    final IconData icon;
    if (strength >= 70) {
      icon = Icons.signal_wifi_4_bar;
    } else if (strength >= 40) {
      icon = Icons.network_wifi_3_bar;
    } else {
      icon = Icons.network_wifi_1_bar;
    }
    return Icon(icon);
  }
}
