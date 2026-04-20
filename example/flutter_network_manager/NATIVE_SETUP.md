# Native Library Setup - Flutter Way (CMake)

This guide explains how the native library is bundled with your Flutter Linux app.

## What Was Done

Your Flutter project is configured to automatically bundle `libconnman_nc.so` with your app:

1. ✅ Created `linux/libs/` directory
2. ✅ Placed `libconnman_nc.so` in `linux/libs/`
3. ✅ Updated `linux/CMakeLists.txt` to copy the `.so` file into the app bundle during build

## How It Works

### Library Loading (Dart side)
The `library_loader.dart` in `connman_native_comms` looks for the library in this order:

1. **Environment variable**: `CONNMAN_NC_LIB=/path/to/libconnman_nc.so`
2. **Next to executable**: `$exe_dir/libconnman_nc.so`
3. **In `lib/` subdirectory**: `$exe_dir/lib/libconnman_nc.so` ✅ **This is where bundled libraries go**
4. **System library path**: Uses `LD_LIBRARY_PATH` and `ldconfig`

When you bundle the `.so` file via CMake, it lands in the app bundle's `lib/` directory, and the Dart loader automatically finds it there.

### CMake Build Process (linux/CMakeLists.txt)
When you run `flutter run -d linux`:

```cmake
# Our addition checks for the library and installs it
if(EXISTS "${CUSTOM_NATIVE_LIBS_DIR}/libconnman_nc.so")
  install(FILES "${CUSTOM_NATIVE_LIBS_DIR}/libconnman_nc.so"
    DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
    COMPONENT Runtime)
endif()
```

The app bundle structure becomes:
```
build/linux/arm64/debug/bundle/
├── flutter_network_manager (executable)
└── lib/
    ├── libflutter_linux_gtk.so
    └── libconnman_nc.so ← Your bundled library
```

## Workflow

### Initial Setup (Already Done)

```bash
# The native library is already built at:
# /path/to/connman_native_comms/build/libconnman_nc.so

# And copied to:
# example/flutter_network_manager/linux/libs/libconnman_nc.so
```

### Rebuilding After Native Code Changes

If you modify the C++ code in `native/src/`, rebuild and sync:

```bash
# Option A: Use the convenience script (recommended)
cd example/flutter_network_manager
./build_and_sync_native.sh

# Then run:
flutter run -d linux
```

Or **Option B: Manual steps**
```bash
# 1. Rebuild the native library
cd native
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 2. Copy to Flutter project
cp ../build/libconnman_nc.so ../example/flutter_network_manager/linux/libs/

# 3. Run Flutter (CMake will bundle it automatically)
cd ../example/flutter_network_manager
flutter run -d linux
```

### Running the App

```bash
cd example/flutter_network_manager
flutter run -d linux
```

## Troubleshooting

### ❌ "libconnman_nc.so not found" error

**Cause**: The `.so` file isn't in `linux/libs/` or CMake didn't copy it.

**Fix**:
```bash
# 1. Verify the file exists
ls -la linux/libs/libconnman_nc.so

# 2. If missing, rebuild it
./build_and_sync_native.sh

# 3. Clean Flutter build and retry
flutter clean
flutter run -d linux
```

### ❌ "RPATH errors" or "undefined symbol"

This happens if the `.so` was built for a different architecture or is corrupted.

**Fix**:
```bash
# Check the library's dependencies
ldd linux/libs/libconnman_nc.so

# Rebuild from scratch
rm -rf ../native/build
./build_and_sync_native.sh
flutter clean
flutter run -d linux
```

### ✅ Verify the Bundle

After a successful build, check the bundle:

```bash
ls -la build/linux/arm64/debug/bundle/lib/
# Should show: libconnman_nc.so, libflutter_linux_gtk.so, etc.
```

## Distribution

When you build a release:

```bash
flutter build linux --release
```

The final distributable includes everything:
```
build/linux/release/bundle/
├── flutter_network_manager (executable)
└── lib/
    ├── libflutter_linux_gtk.so
    └── libconnman_nc.so  ← Included!
```

Users can extract and run directly—no system library installation needed.

## Notes

- **Debug vs Release**: CMake bundles the library in both modes
- **Architecture**: The `.so` file must match your target architecture (arm64, x86_64, etc.)
- **Dependencies**: If your native library depends on system libraries (like `libsystemd`), ensure they're installed on the target system
- **Alternatives**: If you prefer system-wide installation instead, see `ALTERNATIVE_SYSTEM_SETUP.md`
