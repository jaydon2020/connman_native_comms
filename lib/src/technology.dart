// technology.dart — ConnmanTechnology wrapper holding live D-Bus state.
// Properties are updated in-place via updateProperties() on signal delivery.

import 'client.dart';
import 'ffi/types.dart';

class ConnmanTechnology {
  final ConnmanClient client;
  final String objectPath;
  
  String name;
  String type;
  bool powered;
  bool connected;
  bool tethering;
  String tetheringIdentifier;
  String tetheringPassphrase;

  ConnmanTechnology.internal(this.client, ConnmanTechnologyProps props)
      : objectPath = props.objectPath,
        name = props.name,
        type = props.type,
        powered = props.powered,
        connected = props.connected,
        tethering = props.tethering,
        tetheringIdentifier = props.tetheringIdentifier,
        tetheringPassphrase = props.tetheringPassphrase;

  // INTERNAL use: updates cached properties
  void updateProperties(ConnmanTechnologyProps props) {
    name = props.name;
    type = props.type;
    powered = props.powered;
    connected = props.connected;
    tethering = props.tethering;
    tetheringIdentifier = props.tetheringIdentifier;
    tetheringPassphrase = props.tetheringPassphrase;
  }

  Future<void> setPowered(bool state) async {
    await client.technologySetPowered(objectPath, state);
  }

  Future<void> scan() async {
    await client.technologyScan(objectPath);
  }

  @override
  String toString() => 'ConnmanTechnology(\$name, powered: \$powered, connected: \$connected)';
}
