# AASDK Direct Integration Guide

## Overview

We're integrating AASDK (Android Auto SDK) directly into the Tauri application instead of launching OpenAuto as a separate Qt process. This allows us to:

- Display Android Auto content natively in the Tauri window
- Have full control over video/audio rendering
- Integrate seamlessly with the golf cart dashboard UI
- Better performance and tighter integration

## Current Status

✅ **Completed:**
- Created C wrapper header (`src-tauri/aasdk-wrapper/aasdk_c.h`)
- Created C wrapper skeleton (`src-tauri/aasdk-wrapper/aasdk_c.cpp`)
- Created Rust bindings structure (`src-tauri/src/aasdk_bindings.rs`)
- Rewrote `OpenAutoManager` to use AASDK instead of process launching
- Created documentation and build scripts

🔄 **In Progress:**
- Building AASDK library
- Implementing C wrapper with actual AASDK calls
- Generating Rust bindings
- Integrating video/audio streaming

❌ **Not Started:**
- Video frame rendering in Tauri window
- Audio routing through AudioManager
- USB device discovery
- Touch/input event forwarding

## Setup Instructions

### 1. Install Build Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libusb-1.0-0-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libavfilter-dev
```

### 2. Clone AASDK

```bash
cd src-tauri/aasdk-wrapper
git clone https://github.com/f1xpl/aasdk.git
```

### 3. Build AASDK

```bash
cd src-tauri/aasdk-wrapper
chmod +x build_aasdk.sh
./build_aasdk.sh ./aasdk ./build
```

### 4. Complete C Wrapper Implementation

Edit `src-tauri/aasdk-wrapper/aasdk_c.cpp` to:
- Include actual AASDK headers
- Implement all callback functions
- Wire up video/audio/connection callbacks

### 5. Update build.rs

The `build.rs` file needs to:
- Build the C wrapper (`aasdk_c.cpp`)
- Link against AASDK libraries
- Generate Rust bindings using `bindgen`

### 6. Complete Rust Integration

Update `src-tauri/src/openauto.rs` to:
- Use the actual AASDK bindings
- Handle video frames and render in Tauri
- Route audio to `AudioManager`
- Handle USB device discovery
- Forward touch/button events from frontend

## Architecture

```
┌─────────────────────────────────────────┐
│      React Frontend (Dashboard)         │
│  ┌───────────────────────────────────┐  │
│  │  Android Auto Video Component     │  │
│  │  - Displays video frames          │  │
│  │  - Handles touch input            │  │
│  └───────────────────────────────────┘  │
└──────────────┬──────────────────────────┘
               │ Tauri Commands
┌──────────────▼──────────────────────────┐
│      Rust OpenAutoManager               │
│  ┌───────────────────────────────────┐  │
│  │  - Video frame handler            │  │
│  │  - Audio data handler             │  │
│  │  - Input event handler            │  │
│  │  - USB device management          │  │
│  └───────────────────────────────────┘  │
└──────────────┬──────────────────────────┘
               │ FFI
┌──────────────▼──────────────────────────┐
│      C Wrapper (aasdk_c.cpp)            │
│  - Wraps AASDK C++ API in C interface  │
│  - Callbacks for video/audio/events    │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│      AASDK C++ Library                  │
│  - USB communication (libusb)          │
│  - Android Auto protocol               │
│  - Video/audio streaming               │
└─────────────────────────────────────────┘
```

## Key Components

### C Wrapper (`aasdk_c.cpp`)

Provides a C interface to AASDK's C++ API. Key functions:
- `aasdk_init()` - Initialize AASDK
- `aasdk_start()` - Start Android Auto service
- `aasdk_set_video_callback()` - Set video frame callback
- `aasdk_set_audio_callback()` - Set audio data callback
- `aasdk_send_touch_event()` - Forward touch input

### Rust Manager (`openauto.rs`)

High-level Rust API that:
- Manages AASDK lifecycle
- Handles video frames and sends to frontend
- Routes audio to `AudioManager`
- Processes input events from frontend

### Frontend Integration

The React frontend will:
- Display video frames in a canvas or video element
- Capture touch/click events and send to backend
- Show connection status
- Handle Android Auto UI state

## Next Steps

1. **Complete C wrapper:** Integrate actual AASDK classes and implement all callbacks
2. **Update build system:** Modify `build.rs` to build and link everything
3. **Implement video rendering:** Create a way to display video frames in Tauri window
4. **Implement audio routing:** Connect AASDK audio output to `AudioManager`
5. **Add USB device discovery:** Automatically detect and connect to Android phones
6. **Test with real device:** Connect Android phone and verify all functionality

## Resources

- [AASDK GitHub](https://github.com/f1xpl/aasdk)
- [AASDK Wiki](https://github.com/f1xpl/aasdk/wiki)
- [OpenAuto Project](https://github.com/f1xpl/openauto) (reference implementation)

