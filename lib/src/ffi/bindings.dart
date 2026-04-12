import 'dart:ffi';
import 'package:ffi/ffi.dart';

import 'library_loader.dart';

// Native function signatures (FFI type side).
typedef _InitFunc = IntPtr Function(Pointer<Void>);
typedef _CreateClientFunc = Pointer<Void> Function(Int64);
typedef _DestroyClientFunc = Void Function(Pointer<Void>);
typedef _TechSetPoweredFunc = Void Function(
    Pointer<Void>, Pointer<Utf8>, Bool, Int64);
typedef _TechScanFunc = Void Function(Pointer<Void>, Pointer<Utf8>, Int64);
typedef _ServiceConnectFunc = Void Function(
    Pointer<Void>, Pointer<Utf8>, Int64);
typedef _ServiceDisconnectFunc = Void Function(
    Pointer<Void>, Pointer<Utf8>, Int64);
typedef _ServiceRemoveFunc = Void Function(
    Pointer<Void>, Pointer<Utf8>, Int64);
typedef _ServiceSetAutoConnectFunc = Void Function(
    Pointer<Void>, Pointer<Utf8>, Bool, Int64);
typedef _ServiceSetIpv4Func = Void Function(Pointer<Void>, Pointer<Utf8>,
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Int64);

class ConnmanBindings {
  static final DynamicLibrary _lib = loadConnmanLibrary();

  // Explicit asFunction type arguments drive dart:ffi code generation.
  // Field types are inferred from asFunction so private typedef names are not
  // exposed in the public API surface.
  static final init = _lib
      .lookup<NativeFunction<_InitFunc>>('connman_bridge_init')
      .asFunction<int Function(Pointer<Void>)>();

  static final createClient = _lib
      .lookup<NativeFunction<_CreateClientFunc>>('connman_client_create')
      .asFunction<Pointer<Void> Function(int)>();

  static final destroyClient = _lib
      .lookup<NativeFunction<_DestroyClientFunc>>('connman_client_destroy')
      .asFunction<void Function(Pointer<Void>)>();

  static final techSetPowered = _lib
      .lookup<NativeFunction<_TechSetPoweredFunc>>(
          'connman_technology_set_powered')
      .asFunction<void Function(Pointer<Void>, Pointer<Utf8>, bool, int)>();

  static final techScan = _lib
      .lookup<NativeFunction<_TechScanFunc>>('connman_technology_scan')
      .asFunction<void Function(Pointer<Void>, Pointer<Utf8>, int)>();

  static final serviceConnect = _lib
      .lookup<NativeFunction<_ServiceConnectFunc>>('connman_service_connect')
      .asFunction<void Function(Pointer<Void>, Pointer<Utf8>, int)>();

  static final serviceDisconnect = _lib
      .lookup<NativeFunction<_ServiceDisconnectFunc>>(
          'connman_service_disconnect')
      .asFunction<void Function(Pointer<Void>, Pointer<Utf8>, int)>();

  static final serviceRemove = _lib
      .lookup<NativeFunction<_ServiceRemoveFunc>>('connman_service_remove')
      .asFunction<void Function(Pointer<Void>, Pointer<Utf8>, int)>();

  static final serviceSetAutoConnect = _lib
      .lookup<NativeFunction<_ServiceSetAutoConnectFunc>>(
          'connman_service_set_auto_connect')
      .asFunction<void Function(Pointer<Void>, Pointer<Utf8>, bool, int)>();

  static final serviceSetIpv4Config = _lib
      .lookup<NativeFunction<_ServiceSetIpv4Func>>(
          'connman_service_set_ipv4_config')
      .asFunction<
          void Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>,
              Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, int)>();
}
