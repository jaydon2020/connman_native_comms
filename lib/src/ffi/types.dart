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

  ConnmanManagerProps({
    required this.state,
    required this.offlineMode,
    required this.sessionMode,
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

  ConnmanTechnologyProps({
    required this.objectPath,
    required this.name,
    required this.type,
    required this.powered,
    required this.connected,
    required this.tethering,
    required this.tetheringIdentifier,
    required this.tetheringPassphrase,
  });
}

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

  ConnmanServiceProps({
    required this.objectPath,
    required this.name,
    required this.state,
    required this.type,
    required this.strength,
    required this.favorite,
    required this.immutable,
    required this.autoConnect,
    required this.roaming,
    required this.security,
    required this.nameservers,
    required this.domains,
  });
}

class ConnmanObjectRemoved {
  final String objectPath;
  ConnmanObjectRemoved({required this.objectPath});
}

class ConnmanMethodSuccess {
  final String objectPath;
  ConnmanMethodSuccess({required this.objectPath});
}

class ConnmanError {
  final String objectPath;
  final String name;
  final String message;

  ConnmanError({
    required this.objectPath,
    required this.name,
    required this.message,
  });
}
