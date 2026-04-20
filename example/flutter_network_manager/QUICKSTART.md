# Quick Start - Run Your App

## ✅ Setup Complete

Your native library is now properly bundled with your Flutter app.

## 🚀 Run the App

```bash
cd example/flutter_network_manager
flutter run -d linux
```

That's it! The app will:
1. Build your Flutter code
2. **Automatically include `libconnman_nc.so` in the bundle** (via CMake)
3. Launch on your Linux device

## 📁 What's Changed

```
example/flutter_network_manager/
├── linux/
│   ├── libs/
│   │   └── libconnman_nc.so ← Your native library
│   └── CMakeLists.txt ← Updated to bundle the library
├── build_and_sync_native.sh ← Helper script for rebuilds
└── NATIVE_SETUP.md ← Full technical details
```

## 🔄 If You Modify Native Code

Whenever you change files in `native/src/`:

```bash
# Rebuild and sync
cd example/flutter_network_manager
./build_and_sync_native.sh

# Then run
flutter run -d linux
```

Or manually:
```bash
cd native && cmake -B build && cmake --build build
cp build/libconnman_nc.so ../example/flutter_network_manager/linux/libs/
cd ../example/flutter_network_manager && flutter run -d linux
```

## ❓ Issues?

See `NATIVE_SETUP.md` for troubleshooting (errors, architectures, dependencies, etc.)
