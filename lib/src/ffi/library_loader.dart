// library_loader.dart — DynamicLibrary.open() resolution for libbluez_nc.so.

import 'dart:ffi';
import 'dart:io';

import 'package:path/path.dart' as p;

DynamicLibrary loadConnmanLibrary() {
  // 1. Environment variable override.
  final envPath = Platform.environment['CONNMAN_NC_LIB'];
  if (envPath != null && envPath.isNotEmpty) {
    return DynamicLibrary.open(envPath);
  }

  // 2. Next to the running executable.
  final exeDir = p.dirname(Platform.resolvedExecutable);
  final candidates = [
    p.join(exeDir, 'libconnman_nc.so'),
    p.join(exeDir, 'lib', 'libconnman_nc.so'),
  ];

  for (final path in candidates) {
    if (File(path).existsSync()) {
      return DynamicLibrary.open(path);
    }
  }

  // 3. System library path fallback.
  return DynamicLibrary.open('libconnman_nc.so');
}
