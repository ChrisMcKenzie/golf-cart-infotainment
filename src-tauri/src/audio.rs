// Audio output module for DAC integration
// Note: OutputStream is not Send+Sync, so we use a simpler approach
// that creates streams on-demand or stores only the handle

use rodio::{Decoder, OutputStream, Sink};
use std::io::Cursor;
use std::sync::{Arc, Mutex};

pub struct AudioManager {
    volume: Arc<Mutex<f32>>,
    sink_handle: Arc<Mutex<Option<Arc<Mutex<Sink>>>>>,
}

impl AudioManager {
    pub fn new() -> Result<Self, anyhow::Error> {
        // Create a stream and sink on first use
        // The stream will be created when set_volume is first called
        Ok(Self {
            volume: Arc::new(Mutex::new(1.0)),
            sink_handle: Arc::new(Mutex::new(None)),
        })
    }

    fn get_or_create_sink(&self) -> Result<Arc<Mutex<Sink>>, anyhow::Error> {
        let mut sink_guard = self.sink_handle.lock().unwrap();
        
        if sink_guard.is_none() {
            // Create a new stream and sink
            let (_stream, stream_handle) = OutputStream::try_default()
                .map_err(|e| anyhow::anyhow!("Failed to create audio stream: {}", e))?;
            
            // We need to keep the stream alive, but OutputStream is not Send+Sync
            // So we'll leak it to keep it alive for the lifetime of the program
            // This is acceptable for audio streams that should persist
            std::mem::forget(_stream);
            
            let sink = Sink::try_new(&stream_handle)
                .map_err(|e| anyhow::anyhow!("Failed to create audio sink: {}", e))?;
            
            let sink_arc = Arc::new(Mutex::new(sink));
            *sink_guard = Some(sink_arc.clone());
            Ok(sink_arc)
        } else {
            Ok(sink_guard.as_ref().unwrap().clone())
        }
    }

    pub fn play_audio_data(&self, audio_data: Vec<u8>) -> Result<(), anyhow::Error> {
        let cursor = Cursor::new(audio_data);
        let source = Decoder::new(cursor)
            .map_err(|e| anyhow::anyhow!("Failed to decode audio: {}", e))?;
        
        let sink = self.get_or_create_sink()?;
        let sink_guard = sink.lock().unwrap();
        sink_guard.append(source);
        
        Ok(())
    }

    pub fn set_volume(&self, volume: f32) -> Result<(), anyhow::Error> {
        let volume = volume.max(0.0).min(1.0);
        {
            let mut vol_guard = self.volume.lock().unwrap();
            *vol_guard = volume;
        }
        
        // Apply volume to sink if it exists
        if let Ok(sink) = self.get_or_create_sink() {
            let sink_guard = sink.lock().unwrap();
            sink_guard.set_volume(volume);
        }
        
        Ok(())
    }

    pub fn stop(&self) {
        if let Ok(sink) = self.get_or_create_sink() {
            let sink_guard = sink.lock().unwrap();
            sink_guard.stop();
        }
    }

    pub fn pause(&self) {
        if let Ok(sink) = self.get_or_create_sink() {
            let sink_guard = sink.lock().unwrap();
            sink_guard.pause();
        }
    }

    pub fn play(&self) {
        if let Ok(sink) = self.get_or_create_sink() {
            let sink_guard = sink.lock().unwrap();
            sink_guard.play();
        }
    }

    pub fn is_empty(&self) -> bool {
        if let Ok(sink) = self.get_or_create_sink() {
            let sink_guard = sink.lock().unwrap();
            sink_guard.empty()
        } else {
            true
        }
    }
}

impl Default for AudioManager {
    fn default() -> Self {
        Self::new().unwrap_or_else(|_| {
            // Return a dummy manager if initialization fails
            Self {
                volume: Arc::new(Mutex::new(1.0)),
                sink_handle: Arc::new(Mutex::new(None)),
            }
        })
    }
}

unsafe impl Send for AudioManager {}
unsafe impl Sync for AudioManager {}

