// Golf Cart Infotainment System
// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

mod hardware;
mod audio;
mod openauto;
mod aasdk_bindings;
mod video_decoder;

use hardware::{HardwareManager, HardwareStatus};
use audio::AudioManager;
use openauto::OpenAutoManager;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use tauri::Emitter;

// Global state for hardware managers
struct AppState {
    hardware: Arc<Mutex<HardwareManager>>,
    audio: Arc<Mutex<AudioManager>>,
    openauto: Arc<Mutex<OpenAutoManager>>,
    video_streaming_active: Arc<AtomicBool>,
}

#[tauri::command]
fn get_hardware_status(state: tauri::State<AppState>) -> Result<HardwareStatus, String> {
    let hardware = state.inner().hardware.lock().map_err(|e| format!("Lock error: {}", e))?;
    Ok(hardware.read_status())
}

#[tauri::command]
fn set_audio_volume(state: tauri::State<AppState>, volume: f32) -> Result<(), String> {
    let audio = state.inner().audio.lock().map_err(|e| format!("Lock error: {}", e))?;
    audio.set_volume(volume).map_err(|e| e.to_string())
}

#[tauri::command]
fn start_openauto(state: tauri::State<AppState>) -> Result<(), String> {
    let openauto = state.inner().openauto.lock().map_err(|e| format!("Lock error: {}", e))?;
    openauto.start().map_err(|e| e.to_string())
}

#[tauri::command]
fn stop_openauto(state: tauri::State<AppState>) -> Result<(), String> {
    let openauto = state.inner().openauto.lock().map_err(|e| format!("Lock error: {}", e))?;
    openauto.stop().map_err(|e| e.to_string())
}

#[tauri::command]
fn is_openauto_running(state: tauri::State<AppState>) -> Result<bool, String> {
    let openauto = state.inner().openauto.lock().map_err(|e| format!("Lock error: {}", e))?;
    Ok(openauto.is_running())
}

#[tauri::command]
fn is_openauto_connected(state: tauri::State<AppState>) -> Result<bool, String> {
    let openauto = state.inner().openauto.lock().map_err(|e| format!("Lock error: {}", e))?;
    Ok(openauto.is_connected())
}

#[tauri::command]
async fn start_video_stream(
    state: tauri::State<'_, AppState>,
    app: tauri::AppHandle,
) -> Result<(), String> {
    // Check if already streaming
    if state.video_streaming_active.load(Ordering::SeqCst) {
        return Ok(()); // Already streaming
    }

    let openauto = state.openauto.clone();
    let streaming_flag = state.video_streaming_active.clone();

    // Set streaming flag
    streaming_flag.store(true, Ordering::SeqCst);

    // Spawn background task to emit video frames
    tauri::async_runtime::spawn(async move {
        eprintln!("Video streaming task started");

        while streaming_flag.load(Ordering::SeqCst) {
            // Try to get a frame with a timeout
            let frame = {
                let manager = openauto.lock().unwrap();
                manager.recv_video_frame_timeout(std::time::Duration::from_millis(100))
            };

            if let Some(frame) = frame {
                // For now, just pass raw H264 data to frontend
                // TODO: Decode H264 to RGB in Rust
                use base64::{Engine as _, engine::general_purpose};
                let base64_data = general_purpose::STANDARD.encode(&frame.data);

                // Emit H264 frame to frontend
                if let Err(e) = app.emit(
                    "video-frame",
                    VideoFramePayload {
                        data: base64_data,
                        width: frame.width,
                        height: frame.height,
                        format: "h264".to_string(),
                    }
                ) {
                    eprintln!("Failed to emit video frame: {}", e);
                }
            }

            // Small yield to prevent CPU spinning
            tokio::time::sleep(tokio::time::Duration::from_millis(1)).await;
        }

        eprintln!("Video streaming task stopped");
    });

    Ok(())
}

#[tauri::command]
fn stop_video_stream(state: tauri::State<AppState>) -> Result<(), String> {
    state.video_streaming_active.store(false, Ordering::SeqCst);
    Ok(())
}

#[derive(Clone, serde::Serialize)]
struct VideoFramePayload {
    data: String,  // base64 encoded RGB24 data
    width: u32,
    height: u32,
    format: String,  // "rgb24"
}

// Note: Path management removed as we're using AASDK directly now
// If needed, we can add USB device path management later

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    // Initialize hardware managers
    let hardware_manager = Arc::new(Mutex::new(
        HardwareManager::new().unwrap_or_else(|e| {
            eprintln!("Warning: Could not initialize hardware manager: {}", e);
            HardwareManager::default()
        })
    ));
    
    let audio_manager = Arc::new(Mutex::new(
        AudioManager::new().unwrap_or_else(|e| {
            eprintln!("Warning: Could not initialize audio manager: {}", e);
            AudioManager::default()
        })
    ));
    
    let openauto_manager = Arc::new(Mutex::new(OpenAutoManager::new()));

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(AppState {
            hardware: hardware_manager,
            audio: audio_manager,
            openauto: openauto_manager,
            video_streaming_active: Arc::new(AtomicBool::new(false)),
        })
            .invoke_handler(tauri::generate_handler![
                get_hardware_status,
                set_audio_volume,
                start_openauto,
                stop_openauto,
                is_openauto_running,
                is_openauto_connected,
                start_video_stream,
                stop_video_stream,
            ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
