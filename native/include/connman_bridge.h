// connman_bridge.h — C++ ABI entry points for the Dart FFI layer.
//
// These functions build a completely persistent sdbus-c++ event loop context
// hosting the ConnmanManager, and exporting standard one-shot async methods.

#pragma once

#include <cstdint>

#define CONNMAN_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

// Create a new ConnMan bridge client.
// Initializes the Dart API DL, spawns an sdbus event loop thread, and
// returns an opaque handle.  events_port receives 0x01-0x07 payloads.
// Returns nullptr on failure.
CONNMAN_EXPORT void* connman_client_create(void* dart_api_data,
                                           int64_t events_port);

// Destroy a running ConnMan client natively.
CONNMAN_EXPORT void connman_client_destroy(void* client);

// ── Technology Operations ───────────────────────────────────────────────────

CONNMAN_EXPORT void connman_technology_set_powered(void* client,
                                                   const char* object_path,
                                                   bool powered,
                                                   int64_t result_port);

CONNMAN_EXPORT void connman_technology_scan(void* client,
                                            const char* object_path,
                                            int64_t result_port);

// ── Service Operations ──────────────────────────────────────────────────────

CONNMAN_EXPORT void connman_service_connect(void* client,
                                            const char* object_path,
                                            int64_t result_port);

CONNMAN_EXPORT void connman_service_disconnect(void* client,
                                               const char* object_path,
                                               int64_t result_port);

CONNMAN_EXPORT void connman_service_remove(void* client,
                                           const char* object_path,
                                           int64_t result_port);

CONNMAN_EXPORT void connman_service_set_auto_connect(void* client,
                                                     const char* object_path,
                                                     bool auto_connect,
                                                     int64_t result_port);

CONNMAN_EXPORT void connman_service_set_ipv4_config(void* client,
                                                    const char* object_path,
                                                    const char* method,
                                                    const char* address,
                                                    const char* netmask,
                                                    const char* gateway,
                                                    int64_t result_port);

#ifdef __cplusplus
}
#endif
