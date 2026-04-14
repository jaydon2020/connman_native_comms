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

  // Keyed by objectPath so duplicate signals are idempotent.
  final _services = <String, ConnmanService>{};

  StreamSubscription<ConnmanService>? _addedSub;
  StreamSubscription<ConnmanService>? _changedSub;
  StreamSubscription<ConnmanService>? _removedSub;
  StreamSubscription<ConnmanTechnology>? _techSub;

  // _scanning: true while wifi.scan() is in-flight.
  // _busy: true for the entire async operation (power-on + scan + sync).
  //        Disables the scan button to prevent concurrent calls.
  bool _scanning = false;
  bool _busy = false;
  bool _connected = false;
  String? _error;

  // ── Accessors ─────────────────────────────────────────────────────────────

  ConnmanTechnology? get _wifi =>
      _client.technologies.where((t) => t.type == 'wifi').firstOrNull;

  // Reads from the live object — always up-to-date because technologyChanged
  // calls setState() which re-evaluates this getter.
  bool get _powered => _wifi?.powered ?? false;

  // ── Lifecycle ─────────────────────────────────────────────────────────────

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
        // Seed with services already known to ConnMan (from previous scans /
        // previously connected networks).
        _syncServicesFromClient();
      });

      // These subscriptions keep _services live for the entire widget lifetime.
      _addedSub = _client.serviceAdded.listen(_onServiceEvent);
      _changedSub = _client.serviceChanged.listen(_onServiceEvent);
      _removedSub = _client.serviceRemoved.listen((svc) {
        if (mounted) setState(() => _services.remove(svc.objectPath));
      });

      // Reflect WiFi power-state changes in the UI.
      _techSub = _client.technologyChanged.listen(_onTechChanged);
    } catch (e) {
      if (!mounted) return;
      setState(() => _error = e.toString());
    }
  }

  void _onServiceEvent(ConnmanService svc) {
    if (!mounted || svc.type != 'wifi') return;
    setState(() => _services[svc.objectPath] = svc);
  }

  void _onTechChanged(ConnmanTechnology tech) {
    if (!mounted || tech.type != 'wifi') return;
    setState(() {
      if (!tech.powered) {
        // WiFi turned off — clear stale results and reset scan state.
        _services.clear();
        _scanning = false;
      } else {
        // WiFi just powered on — bring in any services ConnMan already knows.
        _syncServicesFromClient();
      }
    });
  }

  /// Copies all wifi services from the live client cache into [_services].
  /// Does NOT clear first — existing entries are updated in-place, new ones
  /// are added.  This is called both on init and after a scan completes to
  /// catch pre-existing services that didn't emit a fresh ServicesChanged.
  void _syncServicesFromClient() {
    for (final svc in _client.services) {
      if (svc.type == 'wifi') _services[svc.objectPath] = svc;
    }
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

  // ── Actions ───────────────────────────────────────────────────────────────

  Future<void> _startScan() async {
    final wifi = _wifi;
    if (!_connected || wifi == null || _busy) return;

    setState(() => _busy = true);

    try {
      // Power on if needed.  AlreadyEnabled means it was already on — fine.
      if (!_powered) {
        try {
          await wifi.setPowered(true);
        } on ConnmanAlreadyEnabledException {
          // Already enabled — proceed.
        }
        // Give ConnMan a moment to finish bringing the adapter up.
        await Future<void>.delayed(const Duration(milliseconds: 500));
        if (!mounted) return;
      }

      setState(() => _scanning = true);

      await wifi.scan();
      if (!mounted) return;

      // After scan() returns ConnMan has finished its scan cycle.  Sync the
      // full client service list so we capture any pre-existing services that
      // did not trigger a fresh ServicesChanged signal.  This is the fix for
      // "not all networks shown": serviceChanged/serviceAdded events cover new
      // and updated entries live, but some well-known networks may not emit a
      // new signal if their properties didn't change during this scan.
      setState(_syncServicesFromClient);
    } on ConnmanException catch (e) {
      if (mounted) _showSnackBar(e.message);
    } finally {
      if (mounted) setState(() { _busy = false; _scanning = false; });
    }
  }

  void _showSnackBar(String message) {
    ScaffoldMessenger.of(context)
      ..clearSnackBars()
      ..showSnackBar(SnackBar(content: Text(message)));
  }

  // ── Build ─────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    if (_error != null) return _buildError();

    final sorted = _services.values.toList()
      ..sort((a, b) => b.strength.compareTo(a.strength));

    return Scaffold(
      appBar: AppBar(
        title: Text(_scanning ? 'Scanning…' : 'Network Manager'),
        bottom: _scanning
            ? const PreferredSize(
                preferredSize: Size.fromHeight(4),
                child: LinearProgressIndicator(),
              )
            : null,
        actions: [
          if (_connected)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 4),
              child: Icon(
                _powered ? Icons.wifi : Icons.wifi_off,
                color: _powered ? null : Colors.red,
              ),
            ),
          IconButton(
            tooltip: _scanning ? 'Scanning…' : 'Scan for networks',
            icon: Icon(_scanning ? Icons.wifi_find : Icons.wifi_find),
            // Disable while busy to prevent concurrent scan calls.
            onPressed: _connected && !_busy ? _startScan : null,
          ),
        ],
      ),
      body: !_connected
          ? const Center(child: CircularProgressIndicator())
          : !_powered
              ? _buildWifiOff()
              : sorted.isEmpty
                  ? _buildEmpty()
                  : _buildList(sorted),
    );
  }

  Widget _buildWifiOff() {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.wifi_off, size: 64, color: Colors.grey),
          const SizedBox(height: 16),
          const Text('WiFi is powered off.'),
          const SizedBox(height: 24),
          FilledButton.icon(
            onPressed: _busy ? null : _startScan,
            icon: const Icon(Icons.power_settings_new),
            label: const Text('Power On & Scan'),
          ),
        ],
      ),
    );
  }

  Widget _buildEmpty() {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.wifi_find, size: 64,
              color: Theme.of(context).colorScheme.primary),
          const SizedBox(height: 16),
          const Text('No networks found.'),
          const SizedBox(height: 8),
          const Text(
            'Tap the scan button to search.',
            style: TextStyle(color: Colors.grey),
          ),
        ],
      ),
    );
  }

  Widget _buildList(List<ConnmanService> sorted) {
    return ListView.builder(
      itemCount: sorted.length,
      itemBuilder: (context, i) {
        final svc = sorted[i];
        final name = svc.name.isNotEmpty ? svc.name : '(hidden)';
        final isConnected =
            svc.state == 'online' || svc.state == 'ready';
        final isConnecting =
            svc.state == 'association' || svc.state == 'configuration';

        return ListTile(
          leading: _strengthIcon(svc.strength),
          title: Text(name),
          subtitle: Text(
            isConnecting ? '${svc.state}…' : svc.state,
            style: TextStyle(
              color: isConnected ? Colors.green : null,
            ),
          ),
          trailing: isConnected
              ? const Icon(Icons.check_circle, color: Colors.green)
              : isConnecting
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : null,
          onTap: () => Navigator.push(
            context,
            MaterialPageRoute<void>(
              builder: (_) =>
                  ServiceScreen(client: _client, service: svc),
            ),
          ),
        );
      },
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
