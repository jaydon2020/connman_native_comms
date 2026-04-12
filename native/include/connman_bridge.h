// connman_bridge.h — C ABI entry points for the Dart FFI layer.
//
// These functions build a completely persistent sdbus-c++ event loop context
// hosting the ConnmanManager, and exporting standard one-shot async methods.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the bridge (loads Dart_ApiDL). Must be called once before any
// client operations, typically with NativeApi.initializeApiDLData.
intptr_t connman_bridge_init(void* dart_api_data);

// Create a new ConnMan bridge client.
// Spawn a detached std::thread hosting a sdbus::IConnection event loop,
// and initializes a persistent ConnmanManager instance tracking state.
// events_port determines where the 0x01-0x07 payloads are pushed.
void* connman_client_create(int64_t events_port);

// Destroy a running ConnMan client natively.
void connman_client_destroy(void* client);

// ── Technology Operations ───────────────────────────────────────────────────

void connman_technology_set_powered(void* client,
                                    const char* object_path,
                                    bool powered,
                                    int64_t result_port);

void connman_technology_scan(void* client,
                             const char* object_path,
                             int64_t result_port);

// ── Service Operations ──────────────────────────────────────────────────────

void connman_service_connect(void* client,
                             const char* object_path,
                             int64_t result_port);

void connman_service_disconnect(void* client,
                                const char* object_path,
                                int64_t result_port);

void connman_service_remove(void* client,
                            const char* object_path,
                            int64_t result_port);

void connman_service_set_auto_connect(void* client,
                                      const char* object_path,
                                      bool auto_connect,
                                      int64_t result_port);

void connman_service_set_ipv4_config(void* client,
                                     const char* object_path,
                                     const char* method,
                                     const char* address,
                                     const char* netmask,
                                     const char* gateway,
                                     int64_t result_port);

#ifdef __cplusplus
}
#endif
