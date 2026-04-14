// bindings.dart — lookupFunction wrappers for connman_bridge.h C ABI.

import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../internal/library_loader.dart';

/// FFI bindings to the native connman_nc shared library.
class ConnmanBindings {
  ConnmanBindings._();

  static final DynamicLibrary _lib = loadConnmanLibrary();

  static final createClient = _lib.lookupFunction<
      Pointer<Void> Function(Pointer<Void>, Int64),
      Pointer<Void> Function(Pointer<Void>, int)>('connman_client_create');

  static final destroyClient = _lib.lookupFunction<Void Function(Pointer<Void>),
      void Function(Pointer<Void>)>('connman_client_destroy');

  static final techSetPowered = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Bool, Int64),
      void Function(Pointer<Void>, Pointer<Utf8>, bool,
          int)>('connman_technology_set_powered');

  static final techScan = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Int64),
      void Function(
          Pointer<Void>, Pointer<Utf8>, int)>('connman_technology_scan');

  static final serviceConnect = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Int64),
      void Function(
          Pointer<Void>, Pointer<Utf8>, int)>('connman_service_connect');

  static final serviceDisconnect = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Int64),
      void Function(
          Pointer<Void>, Pointer<Utf8>, int)>('connman_service_disconnect');

  static final serviceRemove = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Int64),
      void Function(
          Pointer<Void>, Pointer<Utf8>, int)>('connman_service_remove');

  static final serviceSetAutoConnect = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Bool, Int64),
      void Function(Pointer<Void>, Pointer<Utf8>, bool,
          int)>('connman_service_set_auto_connect');

  static final serviceSetIpv4Config = _lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>,
          Pointer<Utf8>, Pointer<Utf8>, Int64),
      void Function(
          Pointer<Void>,
          Pointer<Utf8>,
          Pointer<Utf8>,
          Pointer<Utf8>,
          Pointer<Utf8>,
          Pointer<Utf8>,
          int)>('connman_service_set_ipv4_config');
}
