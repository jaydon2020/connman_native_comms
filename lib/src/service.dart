// service.dart — ConnmanService wrapper holding live D-Bus state.
// Properties are updated in-place via updateProperties() on signal delivery.

import 'client.dart';
import 'ffi/types.dart';

class ConnmanService {
  final ConnmanClient client;
  final String objectPath;

  String name;
  String state;
  String type;
  int strength;
  bool favorite;
  bool immutable;
  bool autoConnect;
  bool roaming;
  // Unmodifiable to prevent external mutation from desyncing cached state.
  List<String> security;
  List<String> nameservers;
  List<String> domains;

  ConnmanService.internal(this.client, ConnmanServiceProps props)
      : objectPath = props.objectPath,
        name = props.name,
        state = props.state,
        type = props.type,
        strength = props.strength,
        favorite = props.favorite,
        immutable = props.immutable,
        autoConnect = props.autoConnect,
        roaming = props.roaming,
        security = List.unmodifiable(props.security),
        nameservers = List.unmodifiable(props.nameservers),
        domains = List.unmodifiable(props.domains);

  // INTERNAL use: updates cached properties on live D-Bus signal.
  void updateProperties(ConnmanServiceProps props) {
    name = props.name;
    state = props.state;
    type = props.type;
    strength = props.strength;
    favorite = props.favorite;
    immutable = props.immutable;
    autoConnect = props.autoConnect;
    roaming = props.roaming;
    security = List.unmodifiable(props.security);
    nameservers = List.unmodifiable(props.nameservers);
    domains = List.unmodifiable(props.domains);
  }

  Future<void> connect() async {
    await client.serviceConnect(objectPath);
  }

  Future<void> disconnect() async {
    await client.serviceDisconnect(objectPath);
  }

  Future<void> remove() async {
    await client.serviceRemove(objectPath);
  }

  Future<void> setAutoConnect(bool state) async {
    await client.serviceSetAutoConnect(objectPath, state);
  }

  Future<void> setIpv4Config({
    required String method, // "dhcp", "manual", "off"
    String address = '',
    String netmask = '',
    String gateway = '',
  }) async {
    await client.serviceSetIpv4Config(
      objectPath: objectPath,
      method: method,
      address: address,
      netmask: netmask,
      gateway: gateway,
    );
  }

  @override
  String toString() =>
      'ConnmanService($name, state: $state, strength: $strength)';
}
