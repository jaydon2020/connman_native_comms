import 'dart:typed_data';

import 'package:connman_native_comms/src/exceptions.dart';
import 'package:connman_native_comms/src/ffi/codec.dart';
import 'package:connman_native_comms/src/ffi/types.dart';
import 'package:test/test.dart';

// ── Helper ──────────────────────────────────────────────────────────────────
//
// glaze BEVE wire format used throughout these tests:
//   string  : uint32-LE byte-count + UTF-8 bytes (no null terminator)
//   bool    : uint8 (0x00 = false, any non-zero = true)
//   int16   : int16-LE (2 bytes)
//   list<T> : uint32-LE element-count + T elements in sequence

/// Encodes a UTF-8 string as a glaze BEVE length-prefixed byte sequence.
List<int> glazeString(String s) {
  final bytes = s.codeUnits; // ASCII-safe; use utf8.encode for full Unicode
  final len = bytes.length;
  return [
    len & 0xFF,
    (len >> 8) & 0xFF,
    (len >> 16) & 0xFF,
    (len >> 24) & 0xFF,
    ...bytes,
  ];
}

/// Encodes a list of strings as a glaze BEVE string-list.
List<int> glazeStringList(List<String> items) {
  final count = items.length;
  return [
    count & 0xFF,
    (count >> 8) & 0xFF,
    (count >> 16) & 0xFF,
    (count >> 24) & 0xFF,
    for (final s in items) ...glazeString(s),
  ];
}

/// Encodes a little-endian int16.
List<int> glazeInt16(int v) => [v & 0xFF, (v >> 8) & 0xFF];

/// Builds a full message buffer: discriminator byte + payload bytes.
Uint8List msg(int discriminator, List<int> payload) =>
    Uint8List.fromList([discriminator, ...payload]);

void main() {
  // ── Exceptions ─────────────────────────────────────────────────────────────

  group('parseConnmanException', () {
    test('InProgress', () {
      final e =
          parseConnmanException('net.connman.Error.InProgress', '/p', 'msg');
      expect(e, isA<ConnmanInProgressException>());
      expect(e.objectPath, '/p');
      expect(e.message, 'msg');
    });

    test('AlreadyExists', () {
      final e =
          parseConnmanException('net.connman.Error.AlreadyExists', '/p', 'msg');
      expect(e, isA<ConnmanAlreadyExistsException>());
    });

    test('AlreadyConnected', () {
      final e = parseConnmanException(
          'net.connman.Error.AlreadyConnected', '/p', 'msg');
      expect(e, isA<ConnmanAlreadyConnectedException>());
    });

    test('NotConnected', () {
      final e =
          parseConnmanException('net.connman.Error.NotConnected', '/p', 'msg');
      expect(e, isA<ConnmanNotConnectedException>());
    });

    test('NotSupported', () {
      final e =
          parseConnmanException('net.connman.Error.NotSupported', '/p', 'msg');
      expect(e, isA<ConnmanNotSupportedException>());
    });

    test('InvalidArguments', () {
      final e = parseConnmanException(
          'net.connman.Error.InvalidArguments', '/p', 'msg');
      expect(e, isA<ConnmanInvalidArgumentsException>());
    });

    test('Failed', () {
      final e = parseConnmanException('net.connman.Error.Failed', '/p', 'msg');
      expect(e, isA<ConnmanFailedException>());
    });

    test('OperationTimeout', () {
      final e = parseConnmanException(
          'net.connman.Error.OperationTimeout', '/p', 'msg');
      expect(e, isA<ConnmanOperationTimeoutException>());
    });

    test('PermissionDenied', () {
      final e = parseConnmanException(
          'net.connman.Error.PermissionDenied', '/p', 'msg');
      expect(e, isA<ConnmanPermissionDeniedException>());
    });

    test('NotRegistered', () {
      final e =
          parseConnmanException('net.connman.Error.NotRegistered', '/p', 'msg');
      expect(e, isA<ConnmanNotRegisteredException>());
    });

    test('PassphraseRequired', () {
      final e = parseConnmanException(
          'net.connman.Error.PassphraseRequired', '/p', 'msg');
      expect(e, isA<ConnmanPassphraseRequiredException>());
    });

    test('unknown error name falls back to base ConnmanException', () {
      final e = parseConnmanException('org.some.Unknown.Error', '/p', 'msg');
      expect(e, isA<ConnmanException>());
      expect(e, isNot(isA<ConnmanFailedException>()));
    });

    test('toString uses runtimeType not hardcoded class name', () {
      final e = ConnmanAlreadyConnectedException('/p', 'msg');
      expect(e.toString(), contains('ConnmanAlreadyConnectedException'));
      expect(e.toString(), isNot(contains('ConnmanException(')));
    });
  });

  // ── GlazeCodec ─────────────────────────────────────────────────────────────

  group('GlazeCodec', () {
    // ── ConnmanMethodSuccess ──────────────────────────────────────────────

    test('decodes ConnmanMethodSuccess via kDone discriminator', () {
      final decoded =
          GlazeCodec.decodePayload(msg(MsgTypes.kDone, glazeString('/path')))
              as ConnmanMethodSuccess;
      expect(decoded.objectPath, '/path');
    });

    test('decodes ConnmanMethodSuccess with empty path', () {
      final decoded =
          GlazeCodec.decodePayload(msg(MsgTypes.kDone, glazeString('')))
              as ConnmanMethodSuccess;
      expect(decoded.objectPath, isEmpty);
    });

    // ── ConnmanManagerProps ───────────────────────────────────────────────

    test('decodes ConnmanManagerProps', () {
      // state: "idle", offlineMode: false, sessionMode: true
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kManagerProps, [
        ...glazeString('idle'),
        0x00, // offlineMode
        0x01, // sessionMode
      ])) as ConnmanManagerProps;
      expect(decoded.state, 'idle');
      expect(decoded.offlineMode, isFalse);
      expect(decoded.sessionMode, isTrue);
    });

    test('decodes ConnmanManagerProps with all defaults', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kManagerProps, [
        ...glazeString(''),
        0x00, // offlineMode
        0x00, // sessionMode
      ])) as ConnmanManagerProps;
      expect(decoded.state, isEmpty);
      expect(decoded.offlineMode, isFalse);
      expect(decoded.sessionMode, isFalse);
    });

    // ── ConnmanTechnologyProps ────────────────────────────────────────────

    test('decodes ConnmanTechnologyProps', () {
      // objectPath: "/p", name: "WiFi", type: "wifi",
      // powered: true, connected: true, tethering: false,
      // tetheringIdentifier: "", tetheringPassphrase: ""
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kTechnologyProps, [
        ...glazeString('/p'),
        ...glazeString('WiFi'),
        ...glazeString('wifi'),
        0x01, // powered
        0x01, // connected
        0x00, // tethering
        ...glazeString(''),
        ...glazeString(''),
      ])) as ConnmanTechnologyProps;
      expect(decoded.objectPath, '/p');
      expect(decoded.name, 'WiFi');
      expect(decoded.type, 'wifi');
      expect(decoded.powered, isTrue);
      expect(decoded.connected, isTrue);
      expect(decoded.tethering, isFalse);
      expect(decoded.tetheringIdentifier, isEmpty);
      expect(decoded.tetheringPassphrase, isEmpty);
    });

    test('kTechnologyAdded uses same TechnologyProps decoder', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kTechnologyAdded, [
        ...glazeString('/net/connman/technology/ethernet'),
        ...glazeString('Wired'),
        ...glazeString('ethernet'),
        0x01, // powered
        0x01, // connected
        0x00, // tethering
        ...glazeString(''),
        ...glazeString(''),
      ])) as ConnmanTechnologyProps;
      expect(decoded.type, 'ethernet');
      expect(decoded.connected, isTrue);
    });

    test('decodes ConnmanTechnologyProps with tethering fields populated', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kTechnologyProps, [
        ...glazeString('/net/connman/technology/wifi'),
        ...glazeString('WiFi'),
        ...glazeString('wifi'),
        0x01, // powered
        0x00, // connected
        0x01, // tethering
        ...glazeString('MyHotspot'),
        ...glazeString('secret123'),
      ])) as ConnmanTechnologyProps;
      expect(decoded.tethering, isTrue);
      expect(decoded.tetheringIdentifier, 'MyHotspot');
      expect(decoded.tetheringPassphrase, 'secret123');
    });

    // ── ConnmanServiceProps ───────────────────────────────────────────────

    test('decodes ConnmanServiceProps', () {
      // strength: 75, security: ["psk"], nameservers: [], domains: []
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kServiceProps, [
        ...glazeString('/net/connman/service/wifi_home'),
        ...glazeString('HomeNet'),
        ...glazeString('ready'),
        ...glazeString('wifi'),
        ...glazeInt16(75), // strength
        0x01, // favorite
        0x00, // immutable
        0x01, // autoConnect
        0x00, // roaming
        ...glazeStringList(['psk']),
        ...glazeStringList([]),
        ...glazeStringList([]),
      ])) as ConnmanServiceProps;
      expect(decoded.objectPath, '/net/connman/service/wifi_home');
      expect(decoded.name, 'HomeNet');
      expect(decoded.state, 'ready');
      expect(decoded.type, 'wifi');
      expect(decoded.strength, 75);
      expect(decoded.favorite, isTrue);
      expect(decoded.immutable, isFalse);
      expect(decoded.autoConnect, isTrue);
      expect(decoded.roaming, isFalse);
      expect(decoded.security, ['psk']);
      expect(decoded.nameservers, isEmpty);
      expect(decoded.domains, isEmpty);
    });

    test('kServiceChanged uses same ServiceProps decoder', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kServiceChanged, [
        ...glazeString('/net/connman/service/wifi_home'),
        ...glazeString('HomeNet'),
        ...glazeString('online'),
        ...glazeString('wifi'),
        ...glazeInt16(90),
        0x01, 0x00, 0x01, 0x00, // fav, immut, autoConn, roaming
        ...glazeStringList([]),
        ...glazeStringList([]),
        ...glazeStringList([]),
      ])) as ConnmanServiceProps;
      expect(decoded.state, 'online');
      expect(decoded.strength, 90);
    });

    test(
        'decodes ConnmanServiceProps with multiple security methods and nameservers',
        () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kServiceProps, [
        ...glazeString('/net/connman/service/wifi_corp'),
        ...glazeString('CorpNet'),
        ...glazeString('ready'),
        ...glazeString('wifi'),
        ...glazeInt16(60),
        0x01,
        0x00,
        0x01,
        0x00,
        ...glazeStringList(['ieee8021x', 'wps']),
        ...glazeStringList(['8.8.8.8', '8.8.4.4']),
        ...glazeStringList(['corp.local']),
      ])) as ConnmanServiceProps;
      expect(decoded.security, ['ieee8021x', 'wps']);
      expect(decoded.nameservers, ['8.8.8.8', '8.8.4.4']);
      expect(decoded.domains, ['corp.local']);
    });

    test('decodes ConnmanServiceProps with negative strength (dBm)', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kServiceProps, [
        ...glazeString('/p'),
        ...glazeString(''),
        ...glazeString(''),
        ...glazeString(''),
        ...glazeInt16(-42),
        0x00,
        0x00,
        0x00,
        0x00,
        ...glazeStringList([]),
        ...glazeStringList([]),
        ...glazeStringList([]),
      ])) as ConnmanServiceProps;
      expect(decoded.strength, -42);
    });

    // ── ConnmanObjectRemoved ──────────────────────────────────────────────

    test('decodes ConnmanObjectRemoved via kServiceRemoved discriminator', () {
      final decoded = GlazeCodec.decodePayload(msg(
              MsgTypes.kServiceRemoved, glazeString('/net/connman/service/x')))
          as ConnmanObjectRemoved;
      expect(decoded.objectPath, '/net/connman/service/x');
    });

    test('decodes ConnmanObjectRemoved via kTechnologyRemoved discriminator',
        () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kTechnologyRemoved,
          glazeString('/net/connman/technology/wifi'))) as ConnmanObjectRemoved;
      expect(decoded.objectPath, '/net/connman/technology/wifi');
    });

    // ── ConnmanError ──────────────────────────────────────────────────────

    test('decodes ConnmanError via kError discriminator', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kError, [
        ...glazeString('/'),
        ...glazeString('E'),
        ...glazeString('M'),
      ])) as ConnmanError;
      expect(decoded.objectPath, '/');
      expect(decoded.name, 'E');
      expect(decoded.message, 'M');
    });

    test('decodes ConnmanError with empty message', () {
      final decoded = GlazeCodec.decodePayload(msg(MsgTypes.kError, [
        ...glazeString('/net/connman/manager'),
        ...glazeString('net.connman.Error.NotRegistered'),
        ...glazeString(''),
      ])) as ConnmanError;
      expect(decoded.name, 'net.connman.Error.NotRegistered');
      expect(decoded.message, isEmpty);
    });

    // ── Unknown discriminator ─────────────────────────────────────────────

    test('decodePayload throws UnimplementedError for unknown discriminator',
        () {
      expect(
        () => GlazeCodec.decodePayload(Uint8List.fromList([0x99])),
        throwsA(isA<UnimplementedError>()),
      );
    });
  });
}
