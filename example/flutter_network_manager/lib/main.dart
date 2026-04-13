import 'package:flutter/material.dart';

import 'scanner_screen.dart';

void main() {
  runApp(const NetworkManagerApp());
}

class NetworkManagerApp extends StatelessWidget {
  const NetworkManagerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Network Manager',
      theme: ThemeData(
        colorSchemeSeed: Colors.teal,
        useMaterial3: true,
      ),
      home: const ScannerScreen(),
    );
  }
}
