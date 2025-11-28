// AASDK Rust bindings - generated from C wrapper
// This module provides Rust FFI bindings to AASDK

use std::os::raw::{c_char, c_int, c_void};

// Opaque handle types (from C header)
pub type AASDKHandle = *mut c_void;

// Wrapper type that can be safely sent between threads
// The C++ implementation handles thread safety internally
#[repr(transparent)]
pub struct AASDKHandleWrapper(pub AASDKHandle);
unsafe impl Send for AASDKHandleWrapper {}
unsafe impl Sync for AASDKHandleWrapper {}
pub type AASDKVideoHandle = *mut c_void;
pub type AASDKAudioHandle = *mut c_void;

// Callback types
pub type VideoFrameCallback = extern "C" fn(
    data: *const u8,
    width: u32,
    height: u32,
    stride: u32,
    user_data: *mut c_void,
);

pub type AudioDataCallback = extern "C" fn(
    samples: *const i16,
    sample_count: u32,
    channels: u32,
    sample_rate: u32,
    user_data: *mut c_void,
);

pub type ConnectionStatusCallback = extern "C" fn(
    connected: bool,
    user_data: *mut c_void,
);

#[link(name = "aasdk_c", kind = "static")]
extern "C" {
    pub fn aasdk_init(
        video_cb: VideoFrameCallback,
        audio_cb: AudioDataCallback,
        conn_cb: ConnectionStatusCallback,
        user_data: *mut c_void,
    ) -> AASDKHandle;
    pub fn aasdk_deinit(handle: AASDKHandle);
    pub fn aasdk_start(handle: AASDKHandle) -> bool;
    pub fn aasdk_stop(handle: AASDKHandle);
    pub fn aasdk_send_touch_event(
        handle: AASDKHandle,
        x: i32,
        y: i32,
        action: i32,
    );
    pub fn aasdk_send_button_event(
        handle: AASDKHandle,
        button_code: i32,
        pressed: bool,
    );
}

