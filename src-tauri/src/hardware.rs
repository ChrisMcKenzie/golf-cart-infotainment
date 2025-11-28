// Hardware interface module for GPIO, signals, and sensors
use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HardwareStatus {
    pub battery_level: f32,        // Battery level as percentage (0.0-100.0)
    pub battery_voltage: f32,      // Battery voltage in volts
    pub drive_mode: DriveMode,     // Current drive mode
    pub headlights_on: bool,       // Headlight status
    pub speed: f32,                // Speed in MPH (or km/h)
    pub signals: SignalStatus,     // Turn signals and other indicators
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum DriveMode {
    Park,
    Reverse,
    Neutral,
    Forward,
    Unknown,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SignalStatus {
    pub left_turn: bool,
    pub right_turn: bool,
    pub hazard: bool,
    pub brake: bool,
}

// GPIO pin configuration (customize these for your wiring)
pub struct PinConfig {
    pub headlight_pin: u8,
    pub drive_mode_pins: (u8, u8), // Two pins for 4-state drive mode
    pub battery_adc_channel: u8,   // ADC channel for battery voltage
    pub left_turn_pin: u8,
    pub right_turn_pin: u8,
    pub brake_pin: u8,
}

impl Default for PinConfig {
    fn default() -> Self {
        Self {
            headlight_pin: 18,        // GPIO 18
            drive_mode_pins: (23, 24), // GPIO 23, 24 for drive mode
            battery_adc_channel: 0,    // MCP3008 channel 0
            left_turn_pin: 25,         // GPIO 25
            right_turn_pin: 8,         // GPIO 8
            brake_pin: 7,              // GPIO 7
        }
    }
}

pub struct HardwareManager {
    #[cfg(target_arch = "arm")]
    gpio: Option<Arc<Mutex<rppal::gpio::Gpio>>>,
    #[cfg(not(target_arch = "arm"))]
    gpio: Option<()>, // Placeholder for non-ARM targets
    pin_config: PinConfig,
    // Store last known values for fallback when GPIO unavailable
    last_status: Arc<Mutex<HardwareStatus>>,
}

impl HardwareManager {
    pub fn new() -> Result<Self, anyhow::Error> {
        let pin_config = PinConfig::default();
        
        #[cfg(target_arch = "arm")]
        let gpio = match rppal::gpio::Gpio::new() {
            Ok(gpio) => {
                // Configure pins as inputs with pull-up resistors
                let gpio_ref = Arc::new(Mutex::new(gpio));
                Self::configure_pins(&gpio_ref, &pin_config)?;
                Some(gpio_ref)
            }
            Err(e) => {
                eprintln!("Warning: Could not initialize GPIO: {}. Running in mock mode.", e);
                None
            }
        };
        
        #[cfg(not(target_arch = "arm"))]
        let gpio = None;

        let last_status = Arc::new(Mutex::new(HardwareStatus {
            battery_level: 75.0,
            battery_voltage: 48.0,
            drive_mode: DriveMode::Neutral,
            headlights_on: false,
            speed: 14.0,
            signals: SignalStatus {
                left_turn: true,
                right_turn: true,
                hazard: false,
                brake: false,
            },
        }));

        Ok(Self {
            gpio,
            pin_config,
            last_status,
        })
    }

    #[cfg(target_arch = "arm")]
    fn configure_pins(
        gpio: &Arc<Mutex<rppal::gpio::Gpio>>,
        config: &PinConfig,
    ) -> Result<(), anyhow::Error> {
        let gpio = gpio.lock().unwrap();
        
        // Configure all input pins
        gpio.get(config.headlight_pin)?.into_input_pullup();
        gpio.get(config.drive_mode_pins.0)?.into_input_pullup();
        gpio.get(config.drive_mode_pins.1)?.into_input_pullup();
        gpio.get(config.left_turn_pin)?.into_input_pullup();
        gpio.get(config.right_turn_pin)?.into_input_pullup();
        gpio.get(config.brake_pin)?.into_input_pullup();
        
        Ok(())
    }

    pub fn read_status(&self) -> HardwareStatus {
        #[cfg(target_arch = "arm")]
        if let Some(gpio) = &self.gpio {
            match self.read_from_gpio(gpio) {
                Ok(status) => {
                    *self.last_status.lock().unwrap() = status.clone();
                    return status;
                }
                Err(e) => {
                    eprintln!("Error reading GPIO: {}", e);
                }
            }
        }

        // Fallback to last known status or defaults
        self.last_status.lock().unwrap().clone()
    }

    #[cfg(target_arch = "arm")]
    fn read_from_gpio(
        &self,
        gpio: &Arc<Mutex<rppal::gpio::Gpio>>,
    ) -> Result<HardwareStatus, anyhow::Error> {
        let gpio = gpio.lock().unwrap();
        
        // Read headlight status (inverted logic if using pull-up)
        let headlights_on = !gpio.get(self.pin_config.headlight_pin)?.read();

        // Read drive mode (2 pins = 4 states)
        let drive_pin0 = !gpio.get(self.pin_config.drive_mode_pins.0)?.read();
        let drive_pin1 = !gpio.get(self.pin_config.drive_mode_pins.1)?.read();
        let drive_mode = match (drive_pin0, drive_pin1) {
            (false, false) => DriveMode::Park,
            (false, true) => DriveMode::Reverse,
            (true, false) => DriveMode::Neutral,
            (true, true) => DriveMode::Forward,
        };

        // Read signals
        let left_turn = !gpio.get(self.pin_config.left_turn_pin)?.read();
        let right_turn = !gpio.get(self.pin_config.right_turn_pin)?.read();
        let brake = !gpio.get(self.pin_config.brake_pin)?.read();

        // Battery voltage reading would go here (via ADC)
        // For now, using a placeholder - you'll need to implement ADC reading
        let battery_voltage = self.read_battery_voltage()?;
        let battery_level = self.calculate_battery_level(battery_voltage);

        // Speed reading would go here (via hall effect sensor, encoder, or GPS)
        // For now, using a placeholder - you'll need to implement speed sensor reading
        let speed = self.read_speed()?;

        Ok(HardwareStatus {
            battery_level,
            battery_voltage,
            drive_mode,
            headlights_on,
            speed,
            signals: SignalStatus {
                left_turn,
                right_turn,
                hazard: left_turn && right_turn,
                brake,
            },
        })
    }

    fn read_battery_voltage(&self) -> Result<f32, anyhow::Error> {
        // TODO: Implement ADC reading via MCP3008 or similar
        // For now, return a mock value
        // Example implementation would use SPI to read from MCP3008
        Ok(48.0) // Placeholder - 48V is typical for golf cart
    }

    fn calculate_battery_level(&self, voltage: f32) -> f32 {
        // Typical golf cart battery: 48V nominal
        // Full charge: ~51V, Empty: ~42V
        // Adjust these values based on your battery specifications
        let min_voltage = 42.0;
        let max_voltage = 51.0;
        
        let percentage = ((voltage - min_voltage) / (max_voltage - min_voltage)) * 100.0;
        percentage.max(0.0).min(100.0)
    }

    fn read_speed(&self) -> Result<f32, anyhow::Error> {
        // TODO: Implement speed reading via hall effect sensor, encoder, or GPS
        // For now, return a mock value
        // Example implementation would read from a speed sensor on a GPIO pin
        // or calculate from wheel rotation sensor
        Ok(0.0) // Placeholder - 0 MPH when stopped
    }
}

impl Default for HardwareManager {
    fn default() -> Self {
        Self::new().unwrap_or_else(|_| {
            // If initialization fails, return a mock manager
            Self {
                #[cfg(target_arch = "arm")]
                gpio: None,
                #[cfg(not(target_arch = "arm"))]
                gpio: None,
                pin_config: PinConfig::default(),
                last_status: Arc::new(Mutex::new(HardwareStatus {
                    battery_level: 75.0,
                    battery_voltage: 48.0,
                    drive_mode: DriveMode::Neutral,
                    headlights_on: false,
                    speed: 0.0,
                    signals: SignalStatus {
                        left_turn: false,
                        right_turn: false,
                        hazard: false,
                        brake: false,
                    },
                })),
            }
        })
    }
}

