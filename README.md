# RFID Door Control System

A secure, WiFi-enabled RFID door control system built with Arduino. The system provides controlled access through RFID card authentication with a centralized server, features automatic door control, and includes both visual and audio feedback.

## Features

- RFID card authentication with secure server communication
- AES-128-CBC encryption for card data transmission
- WiFi connectivity for real-time authorization checks
- Automatic door control using a servo motor
- Visual feedback (green/red LEDs)
- Audio feedback (buzzer)
- Manual override button
- Automatic door closing after configurable delay
- Secure configuration storage
- Debounced input handling

## Hardware Requirements

- Arduino board (with WiFiS3 support)
- MFRC522 RFID reader
- Continuous rotation servo motor
- LEDs (green and red)
- Buzzer
- Push button
- Power supply
- Door mounting hardware

### Pin Configuration

- RFID RC522:
  - RST_PIN: 9
  - SS_PIN: 10
- Status Indicators:
  - GREEN_LED: 7
  - RED_LED: 6
  - BUZZER: 5
- Controls:
  - SERVO_PIN: 3
  - BUTTON_PIN: 2

## Software Dependencies

### Required Libraries
- MFRC522
- Servo
- WiFiS3
- ArduinoJson
- ArduinoBearSSL
- AES128

### Configuration

Create a `arduino_secrets.h` file with the following parameters:
```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
#define SERVER_ADDRESS "your_server_ip"
#define SERVER_PORT 8080
#define DEVICE_UUID "your_device_uuid"
#define AES_KEY { /* your 16-byte AES key */ }
```

## Security Features

1. **Encrypted Communication**
   - AES-128-CBC encryption for RFID card data
   - Random IV generation for each transaction
   - Secure key storage in separate header file

2. **Server Authentication**
   - Real-time card validation with central server
   - Device UUID verification
   - HTTPS support through ArduinoBearSSL

## Door Control System

### Operation Modes

1. **RFID Access**
   - Scans RFID cards/fobs
   - Encrypts card data
   - Validates with server
   - Controls door based on authorization

2. **Manual Override**
   - Push button for internal access
   - Debounced input handling
   - Same door control sequence as RFID access

### Door Control Parameters

- Door movement time: 360ms
- Auto-close delay: 3000ms (3 seconds)
- Servo control values:
  - Stop: 90°
  - Open: 0°
  - Close: 180°

## Installation

1. Clone the repository
2. Install required libraries through Arduino Library Manager
3. Configure `arduino_secrets.h` with your network and security settings
4. Upload to your Arduino board
5. Connect hardware according to pin configuration
6. Test system with authorized RFID cards

## Usage

1. **Normal Operation**
   - Present authorized RFID card to reader
   - Wait for green LED and beep for confirmation
   - Door will open automatically
   - Door closes automatically after delay

2. **Manual Exit**
   - Press internal button
   - Door opens immediately
   - Same auto-close behavior applies

3. **Failed Authentication**
   - Red LED and three beeps indicate denied access
   - Door remains closed

## Troubleshooting

### Common Issues

1. **WiFi Connection**
   - System automatically attempts reconnection
   - Check WiFi credentials in `arduino_secrets.h`
   - Verify network availability

2. **Server Communication**
   - Verify server address and port
   - Check network firewall settings
   - Confirm server is operational

3. **Door Operation**
   - Calibrate servo stop position if needed
   - Adjust timing parameters for your setup
   - Check mechanical clearances

### LED Status Indicators

- **Green LED**
  - Solid: Access granted/door open
  - Off: Door closed/system ready

- **Red LED**
  - Flash: Access denied
  - Off: System ready
