// codec.dart — Glaze BEVE binary decoder for ConnMan-to-Dart payloads.
// Mirrors the glaze::encode() output from native/include/glaze_meta.h.

import 'dart:convert';
import 'dart:typed_data';

import 'types.dart';

// Static-only codec: call GlazeCodec.decodePayload(message) with the raw
// Uint8List from the Dart port. The discriminator byte at index 0 is consumed
// here; all decode methods start reading at offset 1.
class GlazeCodec {
  GlazeCodec._();

  static Object decodePayload(Uint8List message) {
    if (message.isEmpty) {
      throw ArgumentError('Empty message buffer');
    }
    final discriminator = message[0];
    final r = _Reader(message, 1);

    switch (discriminator) {
      case MsgTypes.kManagerProps:
        return r._decodeManagerProps();
      case MsgTypes.kTechnologyProps:
        return r._decodeTechnologyProps();
      case MsgTypes.kServiceProps:
        return r._decodeServiceProps();
      case MsgTypes.kServiceChanged:
        // ServicesChanged publishes a full ConnmanServiceProps snapshot.
        return r._decodeServiceProps();
      case MsgTypes.kServiceRemoved:
        return r._decodeObjectRemoved();
      case MsgTypes.kTechnologyAdded:
        return r._decodeTechnologyProps();
      case MsgTypes.kTechnologyRemoved:
        return r._decodeObjectRemoved();
      case MsgTypes.kDone:
        return r._decodeMethodSuccess();
      case MsgTypes.kError:
        return r._decodeError();
      default:
        throw UnimplementedError('Unknown discriminator: $discriminator');
    }
  }
}

// ── Reader ────────────────────────────────────────────────────────────────────

class _Reader {
  final ByteData _data;
  final int _length;
  int _offset;

  _Reader(Uint8List buffer, int offset)
      : _data =
            ByteData.view(buffer.buffer, buffer.offsetInBytes, buffer.length),
        _length = buffer.length,
        _offset = offset;

  void _checkBounds(int needed) {
    if (_offset + needed > _length) {
      throw RangeError(
          'Buffer underrun: need $needed bytes at offset $_offset, '
          'but buffer length is $_length');
    }
  }

  bool _readBool() {
    _checkBounds(1);
    final v = _data.getUint8(_offset) != 0;
    _offset += 1;
    return v;
  }

  int _readInt16() {
    _checkBounds(2);
    // Glaze BEVE is always little-endian.
    final v = _data.getInt16(_offset, Endian.little);
    _offset += 2;
    return v;
  }

  int _readUint32() {
    _checkBounds(4);
    // Glaze BEVE is always little-endian.
    final v = _data.getUint32(_offset, Endian.little);
    _offset += 4;
    return v;
  }

  String _readString() {
    final len = _readUint32();
    _checkBounds(len);
    final str = utf8
        .decode(_data.buffer.asUint8List(_data.offsetInBytes + _offset, len));
    _offset += len;
    return str;
  }

  List<String> _readStringList() {
    final count = _readUint32();
    return List<String>.generate(count, (_) => _readString());
  }

  // ── Struct decoders ────────────────────────────────────────────────────────

  ConnmanManagerProps _decodeManagerProps() {
    return ConnmanManagerProps(
      state: _readString(),
      offlineMode: _readBool(),
      sessionMode: _readBool(),
    );
  }

  ConnmanTechnologyProps _decodeTechnologyProps() {
    return ConnmanTechnologyProps(
      objectPath: _readString(),
      name: _readString(),
      type: _readString(),
      powered: _readBool(),
      connected: _readBool(),
      tethering: _readBool(),
      tetheringIdentifier: _readString(),
      tetheringPassphrase: _readString(),
    );
  }

  // FIELD ORDER CONTRACT: read order must match glz::meta<ConnmanServiceProps>
  // in native/include/connman_types.h.  New fields go at the end of both.
  ConnmanServiceProps _decodeServiceProps() {
    return ConnmanServiceProps(
      objectPath: _readString(),
      name: _readString(),
      state: _readString(),
      type: _readString(),
      strength: _readInt16(),
      favorite: _readBool(),
      immutable: _readBool(),
      autoConnect: _readBool(),
      roaming: _readBool(),
      security: _readStringList(),
      nameservers: _readStringList(),
      domains: _readStringList(),
      error: _readString(),
    );
  }

  ConnmanObjectRemoved _decodeObjectRemoved() {
    return ConnmanObjectRemoved(objectPath: _readString());
  }

  ConnmanMethodSuccess _decodeMethodSuccess() {
    return ConnmanMethodSuccess(objectPath: _readString());
  }

  ConnmanError _decodeError() {
    return ConnmanError(
      objectPath: _readString(),
      name: _readString(),
      message: _readString(),
    );
  }
}
