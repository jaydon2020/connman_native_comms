// types.dart — Dart-side struct mirrors for glaze-decoded ConnMan payloads.
// These match the C++ structs in native/include/connman_types.h.

abstract final class MsgTypes {
  static const int kManagerProps = 0x01;
  static const int kTechnologyProps = 0x02;
  static const int kServiceProps = 0x03;
  static const int kServiceChanged = 0x04;
  static const int kServiceRemoved = 0x05;
  static const int kTechnologyAdded = 0x06;
  static const int kTechnologyRemoved = 0x07;
  static const int kError = 0x20;
  static const int kDone = 0xFF;
}

class ConnmanManagerProps {
  final String state;
  final bool offlineMode;
  final bool sessionMode;

  const ConnmanManagerProps({
    this.state = '',
    this.offlineMode = false,
    this.sessionMode = false,
  });
}

class ConnmanTechnologyProps {
  final String objectPath;
  final String name;
  final String type;
  final bool powered;
  final bool connected;
  final bool tethering;
  final String tetheringIdentifier;
  final String tetheringPassphrase;

  const ConnmanTechnologyProps({
    this.objectPath = '',
    this.name = '',
    this.type = '',
    this.powered = false,
    this.connected = false,
    this.tethering = false,
    this.tetheringIdentifier = '',
    this.tetheringPassphrase = '',
  });
}

// FIELD ORDER CONTRACT: fields must match glz::meta<ConnmanServiceProps>::fields
// in native/include/connman_types.h exactly.  Add new fields at the END of both.
class ConnmanServiceProps {
  final String objectPath;
  final String name;
  final String state;
  final String type;
  final int strength; // int16_t in C++
  final bool favorite;
  final bool immutable;
  final bool autoConnect;
  final bool roaming;
  final List<String> security;
  final List<String> nameservers;
  final List<String> domains;
  final String error; // ConnMan "Error" property: "dhcp-failed", "" if none

  const ConnmanServiceProps({
    this.objectPath = '',
    this.name = '',
    this.state = '',
    this.type = '',
    this.strength = 0,
    this.favorite = false,
    this.immutable = false,
    this.autoConnect = false,
    this.roaming = false,
    this.security = const [],
    this.nameservers = const [],
    this.domains = const [],
    this.error = '',
  });
}

class ConnmanObjectRemoved {
  final String objectPath;
  const ConnmanObjectRemoved({this.objectPath = ''});
}

class ConnmanMethodSuccess {
  final String objectPath;
  const ConnmanMethodSuccess({this.objectPath = ''});
}

class ConnmanError {
  final String objectPath;
  final String name;
  final String message;

  const ConnmanError({
    this.objectPath = '',
    this.name = '',
    this.message = '',
  });
}
