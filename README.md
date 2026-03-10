# Writing Machine - CNC Pen Plotter

A sophisticated ESP32-based CNC pen plotter (writing machine) that combines precise motion control with an intuitive graphical interface. This project demonstrates advanced motor control, G-code parsing, and embedded systems design.

## Overview

The Writing Machine is a computer numerical control (CNC) device designed to autonomously write or draw on surfaces using a servo-controlled pen. It interprets G-code commands to move two stepper motors (X and Y axes) with millimeter-level precision while controlling a servo motor to lift and lower the pen.

### Key Features

- **Dual Stepper Motor Control**: Precision XY motion using NEMA 17 stepper motors
- **Servo-Controlled Pen Lift**: Smooth pen on/off control for clean lines
- **G-code Support**: Full parsing and execution of standard CNC G-code commands
- **Graphical LCD Interface**: 128x64 pixel RepRap Discount Graphical LCD for status display and menu navigation
- **Bluetooth Connectivity**: Wireless communication for command transmission
- **Web Interface**: HTTP/HTTPS server for remote control and monitoring
- **Motion Control**: Velocity ramping and smooth acceleration profiles
- **Partition Management**: Organized flash storage for configuration and firmware

## Hardware Components

### Microcontroller
- **ESP32** - Dual-core 32-bit processor with WiFi, Bluetooth, and rich peripheral support

### Motion Control
- **2× NEMA 17 Stepper Motors** - High-torque bipolar stepper motors for X and Y axes
- **2× A4988 Stepper Drivers** - Current-regulated microstepping drivers for precise motor control
- **Servo Motor** - Continuous servo for pen lift/lower mechanism

### Display
- **RepRap Discount Graphical LCD (RRD-GLCD)** - 128×64 monochrome LCD with ST7920 controller for real-time status display

### Communication
- Integrated WiFi and Bluetooth (ESP32 native)
- Optional USB connectivity for firmware updates

## Software Architecture

### Main Components

#### Motion Control (`motion_controller/`)
Core motor control logic including:
- Stepper motor driver interface
- Acceleration and velocity profiles
- Position tracking and feedback
- Homing and calibration routines

#### Stepper Motor Driver (`stepper/`)
Low-level control of A4988 stepper drivers:
- Step/direction pulse generation
- Microstepping control
- Current limiting

#### Servo Control (`servo/`)
Pen lift mechanism management:
- PWM servo control
- Position presets (up/down)
- Smooth transitions

#### G-code Parser (`gcode_parser/`)
Interprets and executes CNC G-code:
- Standard G-code commands (G0, G1, G28, etc.)
- Coordinate transformations
- Command queuing and execution

#### Display Driver (`st7920/`)
ST7920 LCD controller interface:
- Pixel-level graphics rendering
- Character display support

#### Menu System (`menu/`)
User interface for:
- Configuration management
- Motion commands
- Status monitoring
- Settings adjustment

#### Graphics Engine (`graphics/`)
2D drawing primitives and rendering

#### Font Management (`fonts/`)
Character rendering on LCD display

#### Input Handler (`input/`)
Button and control input processing

#### Communication Interfaces
- **Bluetooth GATT Server** (`gatt_server/`) - BLE communication
- **HTTPS Server** (`https_server/`) - Web-based control interface

#### Configuration (`config/`)
Build-time and runtime configuration management

## Building and Flashing

### Prerequisites
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) (v5.0 or later recommended)
- Compatible serial-to-USB adapter for programming
- CMake 3.5+

### Build Instructions

1. **Configure the project:**
   ```bash
   idf.py set-target esp32
   idf.py menuconfig
   ```

2. **Build:**
   ```bash
   idf.py build
   ```

3. **Flash to device:**
   ```bash
   idf.py flash
   ```

4. **Monitor serial output:**
   ```bash
   idf.py monitor
   ```

### Partition Table
Custom partition table defined in `partitions.csv` for optimized storage allocation.

## Configuration

### Kconfig
Build-time configuration options available via `idf.py menuconfig`:
- Motor parameters (steps per mm, max speed)
- Display settings
- Communication protocols
- Calibration values

### Runtime Configuration
Additional settings can be adjusted through:
- LCD menu interface
- Bluetooth GATT commands
- Web interface

## G-code Support

The machine supports standard CNC G-code including:

| Command | Function |
|---------|----------|
| G0 | Rapid movement (no pen down) |
| G1 | Linear interpolated movement |
| G28 | Home axes |
| M3/M5 | Spindle on/off (pen down/up) |
| M104 | Set temperature (servo position) |
| G4 | Dwell (pause) |

## Pin Configuration

Configure GPIO assignments in the project settings for:
- Stepper motor step/direction pins (A4988 drivers)
- Servo PWM output
- LCD data/control pins (SPI interface)
- Button input pins
- Optional limit switch inputs

## Motion Mechanics

### Coordinate System
- **X-axis**: Left-right motion
- **Y-axis**: Forward-backward motion
- **Z-axis**: Pen up/down (servo-controlled)

### Resolution
Precision depends on:
- NEMA 17 step angle (1.8°)
- A4988 microstepping (up to 16×)
- Mechanical gear ratios
- Lead screw pitch

### Performance
Typical specifications:
- **Max speed**: Configurable (100-500 mm/min typical)
- **Acceleration**: Smooth ramps to prevent vibration
- **Repeatability**: ±0.5 mm typical

## User Interface

### LCD Menu
Navigate with buttons to:
- View current position
- Manual jogging
- Start/pause printing
- Adjust speed
- Configure calibration

### Web Interface
Connect via WiFi for:
- File upload
- Live status monitoring
- Remote start/stop
- Parameter adjustment

### Bluetooth
BLE-capable devices can:
- Send G-code commands
- Monitor machine status
- Adjust parameters on-the-fly

## Calibration

### Homing
Automatic axis homing routine sets reference points (may require limit switches or manual calibration).

### Steps/mm Calibration
Measure physical movement and adjust motor steps-per-mm values via `menuconfig` or runtime settings.

### Servo Calibration
Adjust pen-up and pen-down positions for optimal writing performance.

## Troubleshooting

### Motors Not Moving
- Verify A4988 wiring and enable pins
- Check step/direction pin configuration
- Ensure adequate power supply for motors (12V, 2A+ recommended)
- Verify microstepping settings

### LCD Display Issues
- Confirm SPI communication pins
- Check backlight power
- Verify ST7920 initialization sequence
- Try hardware reset

### Servo Not Responding
- Verify PWM pin and frequency (50 Hz typical)
- Check servo power supply (5V, 1A+)
- Test servo with known-good PWM signal

### Communication Problems
- Verify WiFi SSID/password in menuconfig
- Check Bluetooth pairing
- Monitor serial output for errors

## Future Enhancements

- [ ] Rotary encoder input for more intuitive control
- [ ] Automatic penhold pressure sensing
- [ ] Multi-color pen selection
- [ ] Speed optimization for faster write rates
- [ ] Temperature-based pen lift compensation
- [ ] Offline SD card support for large G-code files
- [ ] Closed-loop stepper control with encoders

## Project Structure

```
writing_machine/
├── main/              # Application entry point
├── components/
│   ├── stepper/       # A4988 driver control
│   ├── servo/         # Pen lift servo control
│   ├── motion_controller/  # Motion planning & execution
│   ├── gcode_parser/  # G-code interpretation
│   ├── st7920/        # LCD display driver
│   ├── menu/          # User interface
│   ├── graphics/      # Drawing primitives
│   ├── fonts/         # Character rendering
│   ├── input/         # Button input handling
│   ├── gatt_server/   # Bluetooth interface
│   ├── https_server/  # Web control interface
│   └── config/        # Configuration management
├── build/             # Build artifacts
├── partitions.csv     # Custom partition table
├── CMakeLists.txt     # Build configuration
└── README.md          # This file
```

## License

See [LICENSE](LICENSE) file for details.

## Contributing

Contributions welcome! Please ensure:
- Code follows existing style conventions
- All components build without warnings
- Documentation is updated for new features
- Hardware connections are clearly documented

## References

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [A4988 Datasheet](https://www.allegromicro.com/en/products/motor-drivers/stepper-motor-driver-ics/a4988)
- [NEMA 17 Specifications](https://reprap.org/wiki/NEMA_17_Stepper_Motor)
- [ST7920 LCD Controller](https://www.displaytech.com/content/files/datasheets/ST7920.pdf)
- [RepRap GLCD Documentation](https://reprap.org/wiki/RepRap_Discount_Graphical_Liquid_Crystal_Display)
- [G-code Reference](https://www.cnccookbook.com/what-is-gcode/)

## Contact & Support

For issues, questions, or suggestions, please open an issue on the project repository.

---

**Status**: Active Development  
**Last Updated**: December 2025  
**Target Platform**: ESP32 (ESP-IDF)
