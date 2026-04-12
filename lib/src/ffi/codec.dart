import 'dart:convert';
import 'dart:typed_data';

import 'types.dart';

class GlazeCodec {
  final ByteData _data;
  int _offset = 0;

  GlazeCodec(Uint8List buffer)
      : _data = ByteData.view(
            buffer.buffer, buffer.offsetInBytes, buffer.length);

  bool _decodeBool() {
    final v = _data.getUint8(_offset) != 0;
    _offset += 1;
    return v;
  }

  int _decodeInt16() {
    // glaze BEVE is always little-endian.
    final v = _data.getInt16(_offset, Endian.little);
    _offset += 2;
    return v;
  }

  int _decodeUint32() {
    // glaze BEVE is always little-endian.
    final v = _data.getUint32(_offset, Endian.little);
    _offset += 4;
    return v;
  }

  String _decodeString() {
    final len = _decodeUint32();
    final str = utf8.decode(
        _data.buffer.asUint8List(_data.offsetInBytes + _offset, len));
    _offset += len;
    return str;
  }

  List<T> _decodeList<T>(T Function() decoder) {
    final count = _decodeUint32();
    return List<T>.generate(count, (_) => decoder());
  }

  List<String> _decodeStringList() => _decodeList(_decodeString);

  ConnmanManagerProps decodeManagerProps() {
    return ConnmanManagerProps(
      state: _decodeString(),
      offlineMode: _decodeBool(),
      sessionMode: _decodeBool(),
    );
  }

  ConnmanTechnologyProps decodeTechnologyProps() {
    return ConnmanTechnologyProps(
      objectPath: _decodeString(),
      name: _decodeString(),
      type: _decodeString(),
      powered: _decodeBool(),
      connected: _decodeBool(),
      tethering: _decodeBool(),
      tetheringIdentifier: _decodeString(),
      tetheringPassphrase: _decodeString(),
    );
  }

  ConnmanServiceProps decodeServiceProps() {
    return ConnmanServiceProps(
      objectPath: _decodeString(),
      name: _decodeString(),
      state: _decodeString(),
      type: _decodeString(),
      strength: _decodeInt16(),
      favorite: _decodeBool(),
      immutable: _decodeBool(),
      autoConnect: _decodeBool(),
      roaming: _decodeBool(),
      security: _decodeStringList(),
      nameservers: _decodeStringList(),
      domains: _decodeStringList(),
    );
  }

  ConnmanObjectRemoved decodeObjectRemoved() {
    return ConnmanObjectRemoved(objectPath: _decodeString());
  }

  ConnmanMethodSuccess decodeMethodSuccess() {
    return ConnmanMethodSuccess(objectPath: _decodeString());
  }

  ConnmanError decodeError() {
    return ConnmanError(
      objectPath: _decodeString(),
      name: _decodeString(),
      message: _decodeString(),
    );
  }

  Object decodePayload(int discriminator) {
    switch (discriminator) {
      case MsgTypes.kManagerProps:
        return decodeManagerProps();
      case MsgTypes.kTechnologyProps:
        return decodeTechnologyProps();
      case MsgTypes.kServiceProps:
        return decodeServiceProps();
      case MsgTypes.kServiceChanged:
        // ServicesChanged publishes a full ConnmanServiceProps snapshot.
        return decodeServiceProps();
      case MsgTypes.kServiceRemoved:
        return decodeObjectRemoved();
      case MsgTypes.kTechnologyAdded:
        return decodeTechnologyProps();
      case MsgTypes.kTechnologyRemoved:
        return decodeObjectRemoved();
      case MsgTypes.kDone:
        return decodeMethodSuccess();
      case MsgTypes.kError:
        return decodeError();
      default:
        throw UnimplementedError('Unknown discriminator: $discriminator');
    }
  }
}
