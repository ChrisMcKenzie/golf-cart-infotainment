// Golf Cart Infotainment System
// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

mod hardware;
mod audio;
mod openauto;
mod aasdk_bindings;

use hardware::{HardwareManager, HardwareStatus};
use audio::AudioManager;
use openauto::OpenAutoManager;
use std::sync::{Arc, Mutex};

// Global state for hardware managers
struct AppState {
    hardware: Arc<Mutex<HardwareManager>>,
    audio: Arc<Mutex<AudioManager>>,
    openauto: Arc<Mutex<OpenAutoManager>>,
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
        })
            .invoke_handler(tauri::generate_handler![
                get_hardware_status,
                set_audio_volume,
                start_openauto,
                stop_openauto,
                is_openauto_running,
                is_openauto_connected,
            ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
