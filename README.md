# Golf Cart Infotainment System

A custom infotainment dashboard built with Tauri, React, and Rust for Raspberry Pi 5. This system provides real-time monitoring of golf cart signals, battery status, drive mode, and integrates Android Auto via OpenAuto.

## Features

- **Real-time Hardware Monitoring**
  - Battery level and voltage display
  - Drive mode indicator (Park/Reverse/Neutral/Forward)
  - Headlight status
  - Turn signals and brake indicators

- **Audio Output**
  - DAC support for high-quality audio output
  - Volume control

- **Android Auto Integration**
  - OpenAuto C bindings support
  - Full Android Auto functionality

- **Modern Dashboard UI**
  - Responsive design optimized for touch screens
  - Real-time status updates
  - Clean, readable interface

## Hardware Requirements

### Raspberry Pi 5
- Raspberry Pi 5 (4GB or 8GB recommended)
- MicroSD card (32GB+)
- Touchscreen display (HDMI or DSI)
- Power supply (5V/3A minimum)

### GPIO Hardware
- MCP3008 ADC (for battery voltage reading)
- Voltage divider circuit for battery monitoring
- Opto-isolators or voltage dividers for signal inputs (if needed)

### Audio Hardware
- I2S DAC (e.g., HiFiBerry DAC+, JustBoom DAC, etc.)
- Or USB DAC
- Amplifier and speakers

### Wiring

Configure GPIO pins in `src-tauri/src/hardware.rs` (default configuration):

```
GPIO 7  - Brake signal
GPIO 8  - Right turn signal
GPIO 18 - Headlights
GPIO 23 - Drive mode bit 0
GPIO 24 - Drive mode bit 1
GPIO 25 - Left turn signal

SPI/ADC:
- MCP3008 Channel 0 - Battery voltage (via voltage divider)
```

**Important**: Ensure all input signals are properly voltage-leveled (3.3V) for Raspberry Pi GPIO. Use voltage dividers or opto-isolators as needed for 12V/48V signals.

## Software Requirements

### On Raspberry Pi 5

1. **Raspberry Pi OS** (64-bit recommended)
   ```bash
   # Update system
   sudo apt update && sudo apt upgrade -y
   ```

2. **Rust Toolchain**
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   source $HOME/.cargo/env
   rustup target add aarch64-unknown-linux-gnu  # For Pi 5
   ```

3. **System Dependencies**
   ```bash
   sudo apt install -y \
     libasound2-dev \
     pkg-config \
     libssl-dev \
     libgtk-3-dev \
     libwebkit2gtk-4.1-dev \
     libayatana-appindicator3-dev \
     librsvg2-dev
   ```

4. **Node.js and pnpm**
   ```bash
   # Install Node.js (via nvm recommended)
   curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
   nvm install --lts
   
   # Install pnpm
   npm install -g pnpm
   ```

5. **OpenAuto** (optional, for Android Auto)
   ```bash
   # Option 1: Install OpenAuto from source
   # Follow instructions at: https://github.com/f1xpl/openauto
   
   # Option 3: Install via package manager (if available)
   # Check your distribution's repository
   
   # Verify installation:
   which openauto
   # or
   openauto --version
   ```

## Installation

1. **Clone the repository**
   ```bash
   git clone <your-repo-url>
   cd golf-cart-infotainment
   ```

2. **Install frontend dependencies**
   ```bash
   pnpm install
   ```

3. **Configure GPIO pins** (if different from defaults)
   Edit `src-tauri/src/hardware.rs` and adjust the `PinConfig` default values.

4. **Configure battery voltage range**
   Edit `src-tauri/src/hardware.rs` in the `calculate_battery_level` function to match your battery specifications.

5. **Build for Raspberry Pi**
   ```bash
   # Development build
   pnpm tauri dev

   # Production build
   pnpm tauri build
   ```

## Configuration

### GPIO Pin Configuration

Edit `src-tauri/src/hardware.rs` to customize GPIO pin assignments:

```rust
pub struct PinConfig {
    pub headlight_pin: u8,        // Default: 18
    pub drive_mode_pins: (u8, u8), // Default: (23, 24)
    pub battery_adc_channel: u8,   // Default: 0 (MCP3008)
    pub left_turn_pin: u8,         // Default: 25
    pub right_turn_pin: u8,        // Default: 8
    pub brake_pin: u8,             // Default: 7
}
```

### Battery Voltage Calibration

Adjust the voltage thresholds in `calculate_battery_level()`:

```rust
let min_voltage = 42.0;  // Empty battery voltage
let max_voltage = 51.0;  // Full battery voltage
```

### Display Configuration

Edit `src-tauri/tauri.conf.json` to customize window settings:

```json
{
  "app": {
    "windows": [{
      "fullscreen": true,    // Set to true for kiosk mode
      "width": 1280,
      "height": 800,
      "decorations": false
    }]
  }
}
```

### Audio Configuration

The system uses ALSA/PulseAudio by default. Configure your DAC:

1. **For I2S DAC**: Enable in `/boot/config.txt`
   ```
   dtoverlay=hifiberry-dac
   # or
   dtoverlay=justboom-dac
   ```

2. **Set default audio device**:
   ```bash
   alsamixer  # Select your DAC
   sudo alsactl store
   ```

## OpenAuto Integration

The OpenAuto integration automatically searches for OpenAuto in common installation locations and launches it as a subprocess. 

### Setup

1. **Install OpenAuto** on your Raspberry Pi using one of the methods above
2. **Verify Installation**: The system will automatically detect OpenAuto if it's in your PATH or installed in common locations:
   - `/usr/bin/openauto`
   - `/usr/local/bin/openauto`
   - `/opt/openauto/bin/openauto`
   - `/home/pi/openauto/bin/openauto`

3. **Manual Path Configuration** (if needed):
   - If OpenAuto is installed in a non-standard location, you can set it programmatically via the frontend or set the `OPENAUTO_PATH` environment variable

### Features

- ✅ Automatic OpenAuto detection
- ✅ Process lifecycle management
- ✅ Status checking (verifies process is actually running)
- ✅ Graceful shutdown with fallback to force kill
- ✅ Error handling with helpful messages

### Usage

Click the "Start Android Auto" button in the dashboard. The system will:
1. Search for OpenAuto executable
2. Launch it with proper DISPLAY environment variable
3. Monitor the process status
4. Allow you to stop it when needed

### Troubleshooting

If OpenAuto fails to start:
- Check terminal logs for detailed error messages
- Verify OpenAuto is installed: `which openauto`
- Ensure OpenAuto has execute permissions
- Check that DISPLAY environment variable is set correctly
- Verify USB connection to Android phone is working

### Future Enhancements

- Window embedding (C bindings for full integration)
- Multiple OpenAuto window support
- Configuration UI for OpenAuto settings

## Development

### Running in Development Mode

```bash
# Terminal 1: Frontend dev server
pnpm dev

# Terminal 2: Tauri dev (in another terminal)
pnpm tauri dev
```

### Debugging

- Check Rust logs in the terminal
- Frontend console logs in browser DevTools
- GPIO access requires running with appropriate permissions

### GPIO Access Permissions

Add your user to the `gpio` group:

```bash
sudo usermod -a -G gpio $USER
# Log out and back in for changes to take effect
```

Or run with sudo (not recommended for production):

```bash
sudo -E pnpm tauri dev
```

## Building for Production

1. **Create production build**:
   ```bash
   pnpm tauri build
   ```

2. **Output location**: `src-tauri/target/release/bundle/`

3. **Installation on Raspberry Pi**:
   - Copy the `.deb` package from the build output
   - Install: `sudo dpkg -i golf-cart-infotainment_*.deb`

4. **Auto-start on boot**:
   ```bash
   # Create systemd service
   sudo nano /etc/systemd/system/golf-cart-infotainment.service
   ```
   
   Add:
   ```ini
   [Unit]
   Description=Golf Cart Infotainment
   After=graphical.target

   [Service]
   Type=simple
   User=pi
   Environment=DISPLAY=:0
   ExecStart=/usr/bin/golf-cart-infotainment
   Restart=always

   [Install]
   WantedBy=graphical.target
   ```
   
   Enable:
   ```bash
   sudo systemctl enable golf-cart-infotainment
   sudo systemctl start golf-cart-infotainment
   ```

## Troubleshooting

### GPIO Not Working
- Check user permissions (must be in `gpio` group)
- Verify pin assignments match your wiring
- Test with `gpio readall` (if wiringPi is installed)

### Audio Not Working
- Check ALSA configuration: `aplay -l`
- Verify DAC is detected: `dmesg | grep -i audio`
- Test audio: `speaker-test -t wav -c 2`

### OpenAuto Not Starting
- Verify OpenAuto is installed and in PATH
- Check OpenAuto logs
- Ensure USB connection to phone is working

### Battery Level Inaccurate
- Calibrate voltage thresholds in code
- Verify ADC wiring and MCP3008 configuration
- Check voltage divider ratio

## Future Enhancements

- [ ] Complete MCP3008 ADC integration for accurate battery readings
- [ ] Full OpenAuto C bindings integration
- [ ] CAN bus support for modern golf carts
- [ ] GPS integration for speed/distance tracking
- [ ] Data logging and diagnostics
- [ ] Remote monitoring via WiFi

## License

[Your License Here]

## Contributing

[Contributing Guidelines]

## Support

For issues and questions, please open an issue on the GitHub repository.
