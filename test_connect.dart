import 'package:connman_native_comms/connman_native_comms.dart';
import 'dart:io';

void main() async {
  final client = ConnmanClient();
  await client.connect();
  
  final wifiTech = client.technologies.firstWhere((t) => t.type == 'wifi');
  if (!wifiTech.powered) {
    print('WiFi is off, powering on...');
    await wifiTech.setPowered(true);
    await Future.delayed(Duration(seconds: 2));
  }
  
  print('Scanning...');
  try {
    await wifiTech.scan();
  } catch (e) {
    print('Scan error: $e');
  }
  
  await Future.delayed(Duration(seconds: 3));
  
  final services = client.services.where((s) => s.type == 'wifi' && s.name.isNotEmpty).toList();
  if (services.isEmpty) {
    print('No WiFi services found.');
    exit(1);
  }
  
  final svc = services.first;
  print('Trying to connect to ${svc.name} (${svc.objectPath})');
  
  client.agentRequestInput.listen((path) {
    print('Agent requested input for $path');
    print('Please enter password:');
    final pass = stdin.readLineSync();
    if (pass != null && pass.isNotEmpty) {
      client.agentSetPassphrase(path, pass);
    } else {
      client.agentClearPassphrase(path);
    }
  });
  
  try {
    await svc.connect();
    print('Connected successfully!');
  } catch (e) {
    print('Connect failed: $e');
  }
  
  exit(0);
}
