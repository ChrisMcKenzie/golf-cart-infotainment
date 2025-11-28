# AASDK Integration for Golf Cart Infotainment

This directory contains the C wrapper layer for integrating AASDK (Android Auto SDK) directly into the Tauri application.

## Overview

Instead of launching OpenAuto as a separate Qt application, we're integrating AASDK directly to:
- Display Android Auto content natively in the Tauri window
- Have full control over video/audio rendering
- Integrate seamlessly with the golf cart dashboard UI

## Prerequisites

### Build Dependencies

```bash
# On Raspberry Pi / Debian-based systems
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

### AASDK Source

Clone AASDK repository:

```bash
cd src-tauri/aasdk-wrapper
git clone https://github.com/f1xpl/aasdk.git
```

## Building AASDK

1. **Build AASDK library:**
   ```bash
   cd src-tauri/aasdk-wrapper
   chmod +x build_aasdk.sh
   ./build_aasdk.sh ./aasdk ./build
   ```

2. **Build the C wrapper:**
   The C wrapper (`aasdk_c.cpp`) provides a C interface on top of AASDK's C++ API, making it easier to create Rust bindings.

3. **Link everything in build.rs:**
   The `build.rs` file will need to be updated to:
   - Build the C wrapper
   - Link against AASDK libraries
   - Generate Rust bindings using bindgen

## Architecture

```
┌─────────────────────────────────────────┐
│         Tauri Frontend (React)          │
│  ┌───────────────────────────────────┐  │
│  │  Android Auto Video Display       │  │
│  └───────────────────────────────────┘  │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│      Rust OpenAutoManager               │
│  ┌───────────────────────────────────┐  │
│  │  Video Frame Handler              │  │
│  │  Audio Data Handler               │  │
│  │  Input Event Handler              │  │
│  └───────────────────────────────────┘  │
└──────────────┬──────────────────────────┘
               │ FFI
┌──────────────▼──────────────────────────┐
│      C Wrapper (aasdk_c.cpp)            │
│  - Video callbacks                      │
│  - Audio callbacks                      │
│  - Connection callbacks                 │
│  - Input event forwarding               │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│      AASDK C++ Library                  │
│  - USB communication                    │
│  - Android Auto protocol                │
│  - Video/audio streaming                │
└─────────────────────────────────────────┘
```

## Implementation Status

- [x] C wrapper header (`aasdk_c.h`)
- [ ] C wrapper implementation (`aasdk_c.cpp`) - needs AASDK integration
- [ ] Rust bindings (`aasdk_bindings.rs`) - needs bindgen generation
- [ ] Build system integration (`build.rs`)
- [ ] Video frame rendering in Tauri window
- [ ] Audio routing to AudioManager
- [ ] USB device discovery and connection
- [ ] Touch/input event handling

## Next Steps

1. **Complete C wrapper implementation:**
   - Integrate actual AASDK classes
   - Implement video frame callbacks
   - Implement audio data callbacks
   - Implement connection status callbacks

2. **Update build.rs:**
   - Build the C wrapper
   - Link against AASDK
   - Generate Rust bindings

3. **Complete Rust integration:**
   - Handle video frames and render in Tauri
   - Route audio to AudioManager
   - Handle USB device discovery
   - Forward touch/button events

4. **Test:**
   - Connect Android phone via USB
   - Verify video display
   - Verify audio playback
   - Test touch input

## Resources

- [AASDK GitHub Repository](https://github.com/f1xpl/aasdk)
- [AASDK Documentation](https://github.com/f1xpl/aasdk/wiki)
- [Rust FFI Guide](https://doc.rust-lang.org/nomicon/ffi.html)
- [bindgen Documentation](https://rust-lang.github.io/rust-bindgen/)

