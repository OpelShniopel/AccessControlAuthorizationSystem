# RFID Door Control System - Arduino Client

A secure, WiFi-enabled RFID door control system built with Arduino. The system provides controlled access through RFID card authentication by communicating with a centralized authentication server, featuring automatic door control with visual feedback, audio feedback, and unauthorized access monitoring.

## Features
- RFID card authentication via secure server communication
- AES-128-CBC encryption for card data transmission
- WiFi connectivity for real-time authorization checks
- Automatic door control using a servo motor
- Visual feedback (green/red LEDs and LCD display)
- Audio feedback (buzzer)
- Manual override button
- Automatic door closing after configurable delay
- Secure configuration storage
- Debounced input handling
- Photo capture of unauthorized access attempts
- LCD status display

## System Requirements

### Hardware Requirements
- Arduino UNO R4 WiFi board
- MFRC522 RFID reader
- ArduCAM OV5642 camera module
- SD card module
- LCD display (I2C interface)
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
- ArduCAM:
  - CS_PIN: 7
- SD Card:
  - CS_PIN: 8
- Status Indicators:
  - GREEN_LED: 4
  - RED_LED: 6
  - BUZZER: 5
- Controls:
  - SERVO_PIN: 3
  - BUTTON_PIN: 2

### Software Dependencies
Required Arduino Libraries:
- MFRC522
- Servo
- WiFiS3
- ArduinoJson
- ArduinoBearSSL
- AES128
- ArduCAM
- SD
- LiquidCrystal_I2C

### Server Requirement
This client requires a compatible authentication server. The server implementation can be found at:
[https://github.com/jmartynas/kiberfizines-grupinis](https://github.com/jmartynas/kiberfizines-grupinis)

## Configuration

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
   - Random IV generation for each transaction using hardware TRNG
   - Secure key storage in separate header file
   - PKCS7 padding for encryption

2. **Device Authentication**
   - Device UUID verification with server
   - Encrypted card data transmission

3. **Access Monitoring**
   - Photo capture of unauthorized access attempts
   - Images stored on SD card with timestamp
   - 320x240 JPEG format

## Door Control System

### Operation Modes
1. **RFID Access**
   - Scans RFID cards/fobs
   - Encrypts card data with random IV
   - Validates with server
   - Controls door based on authorization
   - Captures photo on unauthorized attempts

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

### User Feedback
- LCD Display Messages:
  - "Ready: Scan Card"
  - "Access Granted!"
  - "Access Denied!"
- LED Indicators:
  - Green: Access granted/door open
  - Red: Access denied (flashing)
- Buzzer Patterns:
  - Access Granted: Single 2000Hz beep
  - Access Denied: Three 500Hz beeps

## Installation & Setup

1. Install required libraries through Arduino Library Manager
2. Configure `arduino_secrets.h` with your network and security settings
3. Ensure the authentication server is set up and running
4. Upload to your Arduino board
5. Connect hardware according to pin configuration
6. Test system with authorized RFID cards
7. Verify photo capture functionality

## Troubleshooting

### Compilation Issues for Renesas Platform

When compiling for Renesas boards (UNO R4 WiFi), you might encounter compilation errors due to C++ reserved words in the `r_sce_if.h` file. This file is included through dependencies and needs to be modified to compile successfully.

1. **Locate the `r_sce_if.h` file:**
   
   ```bash
   /Users/{username}/Library/Arduino15/packages/arduino/hardware/renesas_uno/1.1.0/variants/UNOWI/includes/ra/fsp/src/r_sce/crypto_procedures/src/sce5/plainkey/public/inc/r_sce_if.h
   ```

2. **Required Changes:**
   - Replace C++ reserved words `public` and `private` in struct definitions
   - Example fix for RSA key pair structure:
   ```c
   /* RSA 1024bit key pair index structure */
   typedef struct sce_rsa1024_key_pair_index
   {
       sce_rsa1024_private_key_index_t    priv_key;  // Changed from 'private'
       sce_rsa1024_public_key_index_t     pub_key;   // Changed from 'public'
   } sce_rsa1024_key_pair_index_t;
   ```
   - Search for and replace all other instances of these reserved words in the file

3. **PlatformIO Users:**
   - The same file modification is required when using PlatformIO
   - The file location might vary based on your PlatformIO installation and project configuration
   - Make sure to clean and rebuild the project after making these changes

### ArduCAM Compatibility for UNO R4

The default ArduCAM library was written for Arduino UNO R3 and requires modification to work with the UNO R4 WiFi board. You'll need to:

1. Replace the existing `ArduCAM.h` file in your ArduCAM library with the updated version from:
   [https://github.com/keeeal/ArduCAM-Arduino-Uno-R4/blob/master/ArduCAM/ArduCAM.h](https://github.com/keeeal/ArduCAM-Arduino-Uno-R4/blob/master/ArduCAM/ArduCAM.h)

2. The updated file includes necessary modifications for compatibility with the Renesas architecture used in the UNO R4.

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

4. **Camera/SD Card Issues**
   - Verify SD card is properly formatted (FAT32)
   - Check camera module connections
   - Ensure sufficient power supply

### Status Indicators Guide
- **LCD Display**
  - Shows current system status and feedback
  - Error messages for troubleshooting
- **Green LED**
  - Solid: Access granted/door open
  - Off: Door closed/system ready
- **Red LED**
  - Flash: Access denied
  - Off: System ready
- **Buzzer**
  - Single beep: Access granted
  - Triple beep: Access denied