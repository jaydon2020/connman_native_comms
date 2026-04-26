import 'dart:async';

import 'package:connman_native_comms/connman_native_comms.dart';
import 'package:flutter/material.dart';

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
  // True while a connect() D-Bus call is in-flight (before ConnMan acks it).
  // After ack, _svc.state drives the UI (association → configuration → online).
  bool _awaitingConnectAck = false;

  StreamSubscription<ConnmanService>? _changedSub;
  StreamSubscription<ConnmanTechnology>? _techSub;
  StreamSubscription<String>? _agentSub;
  StreamSubscription<(String, String)>? _agentErrorSub;

  ConnmanService get _svc => widget.service;

  // Derived state — reads from the live mutated object.
  bool get _isConnected => _svc.state == 'online' || _svc.state == 'ready';
  bool get _isConnecting =>
      _awaitingConnectAck ||
      _svc.state == 'association' ||
      _svc.state == 'configuration';

  @override
  void initState() {
    super.initState();
    _changedSub = widget.client.serviceChanged.listen(_onServiceChanged);
    _techSub = widget.client.technologyChanged.listen(_onTechChanged);
    _agentSub = widget.client.agentRequestInput.listen(_onAgentRequestInput);
    _agentErrorSub = widget.client.agentReportError.listen(_onAgentError);
  }

  void _onServiceChanged(ConnmanService svc) {
    if (!mounted || svc.objectPath != _svc.objectPath) return;

    // Once ConnMan acks a terminal state, clear the "awaiting ack" spinner.
    const terminalStates = {'online', 'ready', 'idle', 'failure', 'disconnect'};
    if (terminalStates.contains(svc.state)) {
      _awaitingConnectAck = false;
    }

    setState(() {});
  }

  void _onTechChanged(ConnmanTechnology tech) {
    if (!mounted || tech.type != 'wifi') return;
    if (!tech.powered) {
      // WiFi powered off — go back to root.
      Navigator.of(context).popUntil((route) => route.isFirst);
    }
  }

  void _onAgentRequestInput(String path) {
    if (!mounted || path != _svc.objectPath) return;
    _showPassphraseDialog();
  }

  void _onAgentError((String, String) error) {
    if (!mounted || error.$1 != _svc.objectPath) return;
    _showSnackBar('Connection failed: ${error.$2}');
    setState(() {
      _awaitingConnectAck = false;
    });
  }

  Future<void> _showPassphraseDialog() async {
    final passController = TextEditingController();
    final identityController = TextEditingController(text: 'anonymous');
    
    final result = await showDialog<(String, String)>(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        title: const Text('Authentication Required'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text('Connecting to ${_svc.name}'),
            const SizedBox(height: 16),
            TextField(
              controller: identityController,
              decoration: const InputDecoration(
                labelText: 'Identity (optional)',
                hintText: 'anonymous',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: passController,
              obscureText: true,
              autofocus: true,
              decoration: const InputDecoration(
                labelText: 'Passphrase',
                border: OutlineInputBorder(),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(context, (identityController.text, passController.text)),
            child: const Text('Connect'),
          ),
        ],
      ),
    );

    if (!mounted) return;

    if (result == null) {
      widget.client.agentClearPassphrase(_svc.objectPath);
      setState(() => _awaitingConnectAck = false);
    } else {
      // NOTE: Our current FFI only takes one string. We'll use the passphrase.
      // The native side is currently hardcoded to use "anonymous" for identity
      // if requested by ConnMan, which is standard.
      widget.client.agentSetPassphrase(_svc.objectPath, result.$2);
    }
  }

  Future<void> _connect() async {
    if (_isConnecting || _isConnected) return;
    setState(() {
      _awaitingConnectAck = true;
    });
    try {
      await _svc.connect();
      // Do NOT clear _awaitingConnectAck here — let _onServiceChanged do it
      // when ConnMan reports a terminal state (online/ready/failure/idle).
    } on ConnmanException catch (e) {
      if (mounted) {
        setState(() {
          _awaitingConnectAck = false;
          // If we get an immediate error, ensure the service object reflects it
          _svc.state = 'failure';
        });
        _showSnackBar('Connect failed: ${e.message}');
      }
    }
  }

  Future<void> _disconnect() async {
    if (_isConnecting) return; // don't interrupt an in-progress connect
    try {
      await _svc.disconnect();
    } on ConnmanException catch (e) {
      if (mounted) {
        _showSnackBar('Disconnect failed: ${e.message}');
      }
    }
  }

  void _showSnackBar(String message) {
    ScaffoldMessenger.of(context)
      ..clearSnackBars()
      ..showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  void dispose() {
    _changedSub?.cancel();
    _techSub?.cancel();
    _agentSub?.cancel();
    _agentErrorSub?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final name = _svc.name.isNotEmpty ? _svc.name : '(hidden)';

    return Scaffold(
      appBar: AppBar(
        title: Text(name),
        actions: [_buildActionButton()],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _buildStatusCard(),
          const SizedBox(height: 12),
          _buildPropertiesCard(),
          const SizedBox(height: 12),
        ],
      ),
    );
  }

  Widget _buildActionButton() {
    if (_isConnecting) {
      return const Padding(
        padding: EdgeInsets.all(16),
        child: SizedBox(
          width: 20,
          height: 20,
          child: CircularProgressIndicator(strokeWidth: 2),
        ),
      );
    }
    if (_isConnected) {
      return TextButton(
        onPressed: _disconnect,
        child: const Text('Disconnect'),
      );
    }
    return TextButton(
      onPressed: _connect,
      child: const Text('Connect'),
    );
  }

  Widget _buildStatusCard() {
    Color stateColor;
    IconData stateIcon;
    switch (_svc.state) {
      case 'online':
        stateColor = Colors.green;
        stateIcon = Icons.check_circle;
      case 'ready':
        stateColor = Colors.lightGreen;
        stateIcon = Icons.check_circle_outline;
      case 'association':
      case 'configuration':
        stateColor = Colors.orange;
        stateIcon = Icons.sync;
      case 'failure':
        stateColor = Colors.red;
        stateIcon = Icons.error_outline;
      default: // idle, disconnect
        stateColor = Colors.grey;
        stateIcon = Icons.radio_button_unchecked;
    }

    // Determine the descriptive error message
    String displayError = '';
    if (_svc.state == 'failure') {
        displayError = _svc.error.isNotEmpty ? _svc.error : 'Unknown error';
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(stateIcon, color: stateColor),
                const SizedBox(width: 8),
                Text(
                  _isConnecting ? '${_svc.state}…' : _svc.state,
                  style: Theme.of(context)
                      .textTheme
                      .titleMedium
                      ?.copyWith(color: stateColor),
                ),
              ],
            ),
            // Show ConnMan error reason when the service has failed.
            if (displayError.isNotEmpty) ...[
              const SizedBox(height: 8),
              Row(
                children: [
                  const Icon(Icons.warning_amber, size: 16, color: Colors.red),
                  const SizedBox(width: 4),
                  Expanded(
                    child: Text(
                      'Reason: $displayError',
                      style: const TextStyle(color: Colors.red, fontSize: 13),
                    ),
                  ),
                ],
              ),
            ],
            const SizedBox(height: 8),
            _StrengthBar(strength: _svc.strength),
          ],
        ),
      ),
    );
  }

  Widget _buildPropertiesCard() {
    return Card(
      child: Column(
        children: [
          _PropertyTile('Security',
              _svc.security.isNotEmpty ? _svc.security.join(', ') : 'open'),
          _PropertyTile('Favorite', _svc.favorite ? 'Yes' : 'No'),
          _PropertyTile('Auto-connect', _svc.autoConnect ? 'Yes' : 'No'),
          _PropertyTile('Roaming', _svc.roaming ? 'Yes' : 'No'),
          if (_svc.nameservers.isNotEmpty)
            _PropertyTile('DNS', _svc.nameservers.join(', ')),
          if (_svc.domains.isNotEmpty)
            _PropertyTile('Domains', _svc.domains.join(', ')),
        ],
      ),
    );
  }
}

// ── Strength bar ─────────────────────────────────────────────────────────────

class _StrengthBar extends StatelessWidget {
  final int strength;
  const _StrengthBar({required this.strength});

  @override
  Widget build(BuildContext context) {
    final pct = (strength.clamp(0, 100) / 100.0);
    final color = pct >= 0.7
        ? Colors.green
        : pct >= 0.4
            ? Colors.orange
            : Colors.red;
    return Row(
      children: [
        const Icon(Icons.signal_wifi_4_bar, size: 16, color: Colors.grey),
        const SizedBox(width: 8),
        Expanded(
          child: LinearProgressIndicator(
            value: pct,
            color: color,
            backgroundColor: Colors.grey.shade200,
          ),
        ),
        const SizedBox(width: 8),
        Text('$strength%', style: const TextStyle(fontSize: 12)),
      ],
    );
  }
}

// ── Property tile ─────────────────────────────────────────────────────────────

class _PropertyTile extends StatelessWidget {
  final String label;
  final String value;
  const _PropertyTile(this.label, this.value);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 110,
            child: Text(label,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Colors.grey.shade600,
                    )),
          ),
          Expanded(child: Text(value)),
        ],
      ),
    );
  }
}
