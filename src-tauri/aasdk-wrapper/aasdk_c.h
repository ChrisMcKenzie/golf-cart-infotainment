// C wrapper header for AASDK (Android Auto SDK)
// This provides a C interface to AASDK's C++ implementation

#ifndef AASDK_C_H
#define AASDK_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Opaque handle types
typedef void* AASDKHandle;
typedef void* AASDKVideoHandle;
typedef void* AASDKAudioHandle;

// Callbacks
typedef void (*VideoFrameCallback)(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, void* user_data);
typedef void (*AudioDataCallback)(const int16_t* samples, uint32_t sample_count, uint32_t channels, uint32_t sample_rate, void* user_data);
typedef void (*ConnectionStatusCallback)(bool connected, void* user_data);

// Initialize AASDK with callbacks
// Returns handle on success, NULL on failure
AASDKHandle aasdk_init(VideoFrameCallback video_cb, AudioDataCallback audio_cb, ConnectionStatusCallback conn_cb, void* user_data);

// Start Android Auto service (will auto-discover USB devices)
// Returns true on success, false on failure
bool aasdk_start(AASDKHandle handle);

// Stop Android Auto service
void aasdk_stop(AASDKHandle handle);

// Cleanup AASDK and free all resources
void aasdk_deinit(AASDKHandle handle);

// Send touch event to Android Auto
void aasdk_send_touch_event(AASDKHandle handle, int32_t x, int32_t y, int32_t action);

// Send button event to Android Auto
void aasdk_send_button_event(AASDKHandle handle, int32_t button_code, bool pressed);

#ifdef __cplusplus
}
#endif

#endif // AASDK_C_H

