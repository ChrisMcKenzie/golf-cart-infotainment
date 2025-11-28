// OpenAuto integration module using AASDK directly
// This integrates Android Auto directly into the Tauri app without launching a separate process
use std::sync::{Arc, Mutex, atomic::{AtomicBool, Ordering}};
use std::sync::mpsc::{channel, Sender, Receiver};
use anyhow::Result;
use crate::aasdk_bindings::*;

// Static connection status for callbacks
static CONNECTION_STATUS: AtomicBool = AtomicBool::new(false);

// Static video frame sender for callbacks
static VIDEO_SENDER: Mutex<Option<Sender<VideoFrame>>> = Mutex::new(None);

pub struct OpenAutoManager {
    enabled: Arc<Mutex<bool>>,
    handle: Arc<Mutex<Option<crate::aasdk_bindings::AASDKHandleWrapper>>>,
    video_rx: Arc<Mutex<Option<Receiver<VideoFrame>>>>,
}

#[derive(Clone, serde::Serialize)]
pub struct VideoFrame {
    pub data: Vec<u8>,
    pub width: u32,
    pub height: u32,
    pub stride: u32,
}

impl OpenAutoManager {
    pub fn new() -> Self {
        Self {
            enabled: Arc::new(Mutex::new(false)),
            handle: Arc::new(Mutex::new(None)),
            video_rx: Arc::new(Mutex::new(None)),
        }
    }

    pub fn start(&self) -> Result<()> {
        let mut enabled = self.enabled.lock().unwrap();
        if *enabled {
            return Ok(()); // Already running
        }

        eprintln!("Starting Android Auto via AASDK...");

        // Create video frame channel
        let (tx, rx) = channel::<VideoFrame>();

        // Store the sender in the static mutex for callback access
        {
            let mut sender = VIDEO_SENDER.lock().unwrap();
            *sender = Some(tx);
        }

        // Store the receiver in the manager
        {
            let mut video_rx = self.video_rx.lock().unwrap();
            *video_rx = Some(rx);
        }

        // Initialize AASDK with callbacks
        let handle = unsafe {
            aasdk_init(
                video_frame_callback,
                audio_data_callback,
                connection_status_callback,
                std::ptr::null_mut(), // No user data needed for now
            )
        };

        if handle.is_null() {
            return Err(anyhow::anyhow!("Failed to initialize AASDK"));
        }

        // Store handle
        {
            let mut handle_mutex = self.handle.lock().unwrap();
            *handle_mutex = Some(crate::aasdk_bindings::AASDKHandleWrapper(handle));
        }

        // Start AASDK (this will start USB device discovery)
        let started = unsafe { aasdk_start(handle) };
        if !started {
            unsafe { aasdk_deinit(handle) };
            let mut handle_mutex = self.handle.lock().unwrap();
            *handle_mutex = None;
            return Err(anyhow::anyhow!("Failed to start AASDK"));
        }

        *enabled = true;
        eprintln!("Android Auto started - waiting for USB device connection...");
        Ok(())
    }

    pub fn stop(&self) -> Result<()> {
        let mut enabled = self.enabled.lock().unwrap();
        if !*enabled {
            return Ok(());
        }

        eprintln!("Stopping Android Auto...");

        // Clear video channel
        {
            let mut sender = VIDEO_SENDER.lock().unwrap();
            *sender = None;
        }
        {
            let mut video_rx = self.video_rx.lock().unwrap();
            *video_rx = None;
        }

        // Stop and cleanup AASDK
        let mut handle_mutex = self.handle.lock().unwrap();
        if let Some(handle_wrapper) = handle_mutex.take() {
            let handle = handle_wrapper.0;
            unsafe {
                aasdk_stop(handle);
                aasdk_deinit(handle);
            }
        }

        *enabled = false;
        eprintln!("Android Auto stopped");
        Ok(())
    }

    pub fn is_running(&self) -> bool {
        let enabled = self.enabled.lock().unwrap();
        *enabled
    }
    
    pub fn is_connected(&self) -> bool {
        CONNECTION_STATUS.load(Ordering::SeqCst)
    }

    /// Get the latest video frame (for rendering in Tauri window)
    /// This is a non-blocking call that returns immediately
    pub fn get_video_frame(&self) -> Option<VideoFrame> {
        let video_rx = self.video_rx.lock().unwrap();
        if let Some(ref rx) = *video_rx {
            // Try to receive without blocking
            match rx.try_recv() {
                Ok(frame) => Some(frame),
                Err(_) => None, // No frame available or channel disconnected
            }
        } else {
            None
        }
    }

    /// Try to receive the next video frame, blocking until one is available
    /// Returns None if the channel is closed or timeout
    pub fn recv_video_frame_timeout(&self, timeout: std::time::Duration) -> Option<VideoFrame> {
        let video_rx = self.video_rx.lock().unwrap();
        if let Some(ref rx) = *video_rx {
            match rx.recv_timeout(timeout) {
                Ok(frame) => Some(frame),
                Err(_) => None,
            }
        } else {
            None
        }
    }

    /// Send touch input to Android Auto
    #[allow(dead_code)]
    pub fn send_touch(&self, x: i32, y: i32, action: TouchAction) {
        // TODO: Forward touch event to AASDK
        eprintln!("Touch: ({}, {}) action: {:?}", x, y, action);
    }

    /// Send button press to Android Auto
    #[allow(dead_code)]
    pub fn send_button(&self, button: ButtonCode, pressed: bool) {
        // TODO: Forward button event to AASDK
        eprintln!("Button: {:?} pressed: {}", button, pressed);
    }
}

#[derive(Debug, Clone, Copy)]
#[allow(dead_code)]
pub enum TouchAction {
    Down = 0,
    Up = 1,
    Move = 2,
}

#[derive(Debug, Clone, Copy)]
#[allow(dead_code)]
pub enum ButtonCode {
    Left = 0,
    Right = 1,
    Up = 2,
    Down = 3,
    Enter = 4,
    Back = 5,
    Home = 6,
    Phone = 7,
    CallEnd = 8,
    Microphone = 9,
}

impl Default for OpenAutoManager {
    fn default() -> Self {
        Self::new()
    }
}

// Callback implementations (called from C code)
extern "C" fn video_frame_callback(
    data: *const u8,
    width: u32,
    height: u32,
    buffer_size: u32,
    _user_data: *mut std::ffi::c_void,
) {
    // This will be called from the C wrapper when a new frame arrives
    if data.is_null() {
        return;
    }

    // For H.264 data, buffer_size is the actual compressed data size
    // NOT stride (which would be for raw pixel data)
    let frame_size = buffer_size as usize;

    // Copy the frame data into a Vec
    let frame_data = unsafe {
        std::slice::from_raw_parts(data, frame_size).to_vec()
    };

    // Create the video frame
    let frame = VideoFrame {
        data: frame_data,
        width,
        height,
        stride: buffer_size, // Store buffer size in stride field
    };

    // Send frame through the channel
    if let Ok(sender_guard) = VIDEO_SENDER.lock() {
        if let Some(ref sender) = *sender_guard {
            // Use try_send to avoid blocking if the channel is full
            // This will drop frames if the consumer is too slow
            match sender.send(frame) {
                Ok(_) => {
                    // Frame sent successfully - don't log every frame
                }
                Err(_) => {
                    eprintln!("Warning: Video frame channel full or disconnected");
                }
            }
        }
    }
}

extern "C" fn audio_data_callback(
    samples: *const i16,
    sample_count: u32,
    channels: u32,
    sample_rate: u32,
    _user_data: *mut std::ffi::c_void,
) {
    // This will be called from the C wrapper when new audio samples arrive
    if !samples.is_null() {
        // TODO: Send audio to AudioManager
        eprintln!("Received audio: {} samples, {} channels, {} Hz", 
                 sample_count, channels, sample_rate);
    }
}

extern "C" fn connection_status_callback(
    connected: bool,
    _user_data: *mut std::ffi::c_void,
) {
    // Update connection status using atomic
    CONNECTION_STATUS.store(connected, Ordering::SeqCst);
    eprintln!("Android Auto connection status changed: {}", connected);
}
