/// High-performance Connection Manager (ConnMan) client for Linux using native_comms
/// and sdbus-cpp.
library connman_native_comms;

export 'src/client.dart';
export 'src/exceptions.dart';
export 'src/ffi/types.dart'
    show ConnmanManagerProps, ConnmanTechnologyProps, ConnmanServiceProps;
export 'src/service.dart';
export 'src/technology.dart';
