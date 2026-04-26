// client.dart — high-level ConnMan client backed by the native FFI bridge.
// Owns the event ReceivePort, routes discriminator-tagged messages from C++,
// and exposes typed streams and async methods to callers.

import 'dart:async';
import 'dart:ffi';
import 'dart:isolate';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

import 'exceptions.dart';
import 'ffi/bindings.dart';
import 'ffi/codec.dart';
import 'ffi/types.dart';
import 'service.dart';
import 'technology.dart';

class ConnmanClient {
  ReceivePort? _eventsPort;
  Pointer<Void>? _client;

  // Resolves when the first kManagerProps message arrives (initial snapshot ready).
  Completer<void>? _readyCompleter;

  // State
  ConnmanManagerProps? _managerProps;
  final _technologies = <String, ConnmanTechnology>{};
  final _services = <String, ConnmanService>{};

  // Streams
  final _technologyAddedController =
      StreamController<ConnmanTechnology>.broadcast();
  final _technologyChangedController =
      StreamController<ConnmanTechnology>.broadcast();
  final _technologyRemovedController =
      StreamController<ConnmanTechnology>.broadcast();
  final _serviceAddedController = StreamController<ConnmanService>.broadcast();
  final _serviceChangedController =
      StreamController<ConnmanService>.broadcast();
  final _serviceRemovedController =
      StreamController<ConnmanService>.broadcast();

  // Pending Futures indexed by object path.
  final _pendingMethodCalls = <String, Completer<void>>{};

  ConnmanClient();

  Future<void> connect() async {
    if (_client != null) return;

    _eventsPort = ReceivePort();
    _readyCompleter = Completer<void>();

    // Register the listener before creating the native client so no messages
    // posted during get_managed_objects() are missed.
    _eventsPort!.listen(_handleMessage);

    _client = ConnmanBindings.createClient(
        NativeApi.initializeApiDLData, _eventsPort!.sendPort.nativePort);

    if (_client == nullptr) {
      _eventsPort!.close();
      _eventsPort = null;
      throw StateError('Failed to create native ConnmanClient. '
          'Check D-Bus availability and Dart API DL initialization.');
    }

    // Block until the initial snapshot is delivered (first kManagerProps).
    await _readyCompleter!.future;
  }

  Future<void> close() async {
    // Settle all in-flight callers before tearing down.
    final pending = Map<String, Completer<void>>.from(_pendingMethodCalls);
    _pendingMethodCalls.clear();
    for (final completer in pending.values) {
      completer.completeError(StateError('ConnmanClient closed'));
    }

    if (_client != null && _client != nullptr) {
      ConnmanBindings.destroyClient(_client!);
      _client = null;
    }
    _eventsPort?.close();
    _eventsPort = null;

    await _technologyAddedController.close();
    await _technologyChangedController.close();
    await _technologyRemovedController.close();
    await _serviceAddedController.close();
    await _serviceChangedController.close();
    await _serviceRemovedController.close();
  }

  // ── Accessors ────────────────────────────────────────────────────────────

  ConnmanManagerProps? get managerProps => _managerProps;
  List<ConnmanTechnology> get technologies => _technologies.values.toList();
  List<ConnmanService> get services => _services.values.toList();

  Stream<ConnmanTechnology> get technologyAdded =>
      _technologyAddedController.stream;
  Stream<ConnmanTechnology> get technologyChanged =>
      _technologyChangedController.stream;
  Stream<ConnmanTechnology> get technologyRemoved =>
      _technologyRemovedController.stream;
  Stream<ConnmanService> get serviceAdded => _serviceAddedController.stream;
  Stream<ConnmanService> get serviceChanged => _serviceChangedController.stream;
  Stream<ConnmanService> get serviceRemoved => _serviceRemovedController.stream;

  // ── Message Router ───────────────────────────────────────────────────────

  void _handleMessage(dynamic message) {
    if (message is! Uint8List) return;
    if (message.isEmpty) return;

    final discriminator = message[0];

    // Guard against unknown discriminators (e.g. C++ is newer than Dart) or
    // malformed payloads.  An unhandled throw here would propagate out of the
    // ReceivePort listener and crash the isolate.
    late Object payload;
    try {
      payload = GlazeCodec.decodePayload(message);
    } catch (e, st) {
      assert(
        false,
        'ConnmanClient: failed to decode message '
        '0x${discriminator.toRadixString(16)}: $e\n$st',
      );
      return;
    }

    switch (discriminator) {
      case MsgTypes.kManagerProps:
        _managerProps = payload as ConnmanManagerProps;
        // Signal connect() that the initial snapshot has been delivered.
        _readyCompleter?.complete();
        _readyCompleter = null;
        break;

      case MsgTypes.kTechnologyProps:
        // Sent during the initial snapshot — add new or update existing.
        _upsertTechnology(payload as ConnmanTechnologyProps, fromSignal: false);
        break;

      case MsgTypes.kTechnologyAdded:
        // Live TechnologyAdded signal — treat same as upsert defensively.
        _upsertTechnology(payload as ConnmanTechnologyProps, fromSignal: true);
        break;

      case MsgTypes.kTechnologyRemoved:
        final props = payload as ConnmanObjectRemoved;
        final tech = _technologies.remove(props.objectPath);
        if (tech != null) {
          _technologyRemovedController.add(tech);
        }
        break;

      case MsgTypes.kServiceProps:
        // Sent during the initial snapshot — add new, silently update existing.
        final props = payload as ConnmanServiceProps;
        if (!_services.containsKey(props.objectPath)) {
          final svc = ConnmanService.internal(this, props);
          _services[props.objectPath] = svc;
          _serviceAddedController.add(svc);
        } else {
          _services[props.objectPath]!.updateProperties(props);
        }
        break;

      case MsgTypes.kServiceChanged:
        // Live ServicesChanged signal — add new or notify existing.
        final props = payload as ConnmanServiceProps;
        if (!_services.containsKey(props.objectPath)) {
          final svc = ConnmanService.internal(this, props);
          _services[props.objectPath] = svc;
          _serviceAddedController.add(svc);
        } else {
          _services[props.objectPath]!.updateProperties(props);
          _serviceChangedController.add(_services[props.objectPath]!);
        }
        break;

      case MsgTypes.kServiceRemoved:
        final props = payload as ConnmanObjectRemoved;
        final svc = _services.remove(props.objectPath);
        if (svc != null) {
          _serviceRemovedController.add(svc);
        }
        break;

      case MsgTypes.kDone:
        final props = payload as ConnmanMethodSuccess;
        _pendingMethodCalls.remove(props.objectPath)?.complete();
        break;

      case MsgTypes.kError:
        final props = payload as ConnmanError;
        final exception = parseConnmanException(props.name, props.objectPath, props.message);
        if (_readyCompleter != null) {
          _readyCompleter!.completeError(exception);
          _readyCompleter = null;
        } else {
          _pendingMethodCalls.remove(props.objectPath)?.completeError(exception);
        }
        break;
    }
  }

  void _upsertTechnology(ConnmanTechnologyProps props,
      {required bool fromSignal}) {
    if (!_technologies.containsKey(props.objectPath)) {
      final tech = ConnmanTechnology.internal(this, props);
      _technologies[props.objectPath] = tech;
      _technologyAddedController.add(tech);
    } else {
      _technologies[props.objectPath]!.updateProperties(props);
      if (fromSignal) {
        // Only fire technologyChanged for live signals, not the initial snapshot.
        _technologyChangedController.add(_technologies[props.objectPath]!);
      }
    }
  }

  // ── Dispatch Helper ──────────────────────────────────────────────────────

  /// Registers a pending completer for [objectPath] and executes [nativeCall].
  ///
  /// Returns an error future if the client is closed or an operation for
  /// [objectPath] is already in flight.
  Future<void> _dispatch(String objectPath, void Function() nativeCall) {
    if (_client == null || _client == nullptr) {
      return Future.error(StateError('Client is not connected.'));
    }
    if (_pendingMethodCalls.containsKey(objectPath)) {
      return Future.error(ConnmanInProgressException(
          objectPath, 'Operation already in progress'));
    }
    final completer = Completer<void>();
    _pendingMethodCalls[objectPath] = completer;
    nativeCall();
    return completer.future;
  }

  // ── Actions ──────────────────────────────────────────────────────────────

  Future<void> technologySetPowered(String objectPath, bool powered) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      // NOTE: cPath is freed synchronously after the native call because the
      // C ABI copies the string before returning.  Do not add any await inside
      // this callback — that would free the pointer before C++ finishes reading.
      try {
        ConnmanBindings.techSetPowered(
            _client!, cPath, powered, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
      }
    });
  }

  Future<void> technologyScan(String objectPath) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      try {
        ConnmanBindings.techScan(
            _client!, cPath, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
      }
    });
  }

  Future<void> serviceConnect(String objectPath) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      try {
        ConnmanBindings.serviceConnect(
            _client!, cPath, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
      }
    });
  }

  /// Connects to a secured service by temporarily caching the passphrase in the native agent.
  Future<void> connectWithPassphrase(String objectPath, String passphrase) async {
    agentSetPassphrase(objectPath, passphrase);
    try {
      await serviceConnect(objectPath);
    } finally {
      agentClearPassphrase(objectPath);
    }
  }

  Future<void> serviceDisconnect(String objectPath) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      try {
        ConnmanBindings.serviceDisconnect(
            _client!, cPath, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
      }
    });
  }

  Future<void> serviceRemove(String objectPath) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      try {
        ConnmanBindings.serviceRemove(
            _client!, cPath, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
      }
    });
  }

  Future<void> serviceSetAutoConnect(String objectPath, bool autoConnect) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      try {
        ConnmanBindings.serviceSetAutoConnect(
            _client!, cPath, autoConnect, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
      }
    });
  }

  Future<void> serviceSetIpv4Config({
    required String objectPath,
    required String method,
    required String address,
    required String netmask,
    required String gateway,
  }) {
    return _dispatch(objectPath, () {
      final cPath = objectPath.toNativeUtf8(allocator: calloc);
      final cMethod = method.toNativeUtf8(allocator: calloc);
      final cAddress = address.toNativeUtf8(allocator: calloc);
      final cNetmask = netmask.toNativeUtf8(allocator: calloc);
      final cGateway = gateway.toNativeUtf8(allocator: calloc);
      try {
        ConnmanBindings.serviceSetIpv4Config(_client!, cPath, cMethod, cAddress,
            cNetmask, cGateway, _eventsPort!.sendPort.nativePort);
      } finally {
        calloc.free(cPath);
        calloc.free(cMethod);
        calloc.free(cAddress);
        calloc.free(cNetmask);
        calloc.free(cGateway);
      }
    });
  }

  // ── Agent Operations ──────────────────────────────────────────────────────

  void agentSetPassphrase(String objectPath, String passphrase) {
    if (_client == null || _client == nullptr) return;
    final cPath = objectPath.toNativeUtf8(allocator: calloc);
    final cPassphrase = passphrase.toNativeUtf8(allocator: calloc);
    try {
      ConnmanBindings.agentSetPassphrase(_client!, cPath, cPassphrase);
    } finally {
      calloc.free(cPath);
      calloc.free(cPassphrase);
    }
  }

  void agentClearPassphrase(String objectPath) {
    if (_client == null || _client == nullptr) return;
    final cPath = objectPath.toNativeUtf8(allocator: calloc);
    try {
      ConnmanBindings.agentClearPassphrase(_client!, cPath);
    } finally {
      calloc.free(cPath);
    }
  }
}
