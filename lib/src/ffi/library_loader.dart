import 'dart:ffi';
import 'dart:io';

DynamicLibrary loadConnmanLibrary() {
  if (!Platform.isLinux) {
    throw UnsupportedError('ConnMan is only supported on Linux.');
  }

  try {
    return DynamicLibrary.open('libconnman_nc.so');
  } catch (_) {
    // libconnman_nc.so not found on LD_LIBRARY_PATH; falling back to build path.
    return DynamicLibrary.open('native/build/libconnman_nc.so');
  }
}
