// library_loader.dart — DynamicLibrary.open() resolution for libconnman_nc.so.

import 'dart:ffi';
import 'dart:io';

import 'package:path/path.dart' as p;

DynamicLibrary loadConnmanLibrary() {
  // 1. Environment variable override.
  final envPath = Platform.environment['CONNMAN_NC_LIB'];
  if (envPath != null && envPath.isNotEmpty) {
    return _open(envPath);
  }

  // 2. Next to the running executable.
  final exeDir = p.dirname(Platform.resolvedExecutable);
  final candidates = [
    p.join(exeDir, 'libconnman_nc.so'),
    p.join(exeDir, 'lib', 'libconnman_nc.so'),
  ];

  for (final path in candidates) {
    if (File(path).existsSync()) {
      return _open(path);
    }
  }

  // 3. System library path fallback (LD_LIBRARY_PATH / ldconfig cache).
  return _open('libconnman_nc.so');
}

DynamicLibrary _open(String path) {
  try {
    return DynamicLibrary.open(path);
  } on ArgumentError catch (e) {
    throw StateError(
      'connman_native_comms: could not load "$path".\n'
      'Build the native library first (cd native && cmake -B build && cmake --build build),\n'
      'then either:\n'
      '  • Set CONNMAN_NC_LIB=/absolute/path/to/libconnman_nc.so, or\n'
      '  • Place libconnman_nc.so next to the executable, or\n'
      '  • Install it to a directory on the system library path.\n'
      'Underlying error: $e',
    );
  }
}
