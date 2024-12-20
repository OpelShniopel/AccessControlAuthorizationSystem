#include <Arduino.h>
#include <MFRC522.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <ArduCAM.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "RTC.h"
#include "memorysaver.h"

#include "arduino_secrets.h"
#include "RFIDAuth.h"

// Pins for RFID RC522
#define RST_PIN 9
#define RFID_CS 10

// Pins for ArduCAM and SD Card
#define ARDUCAM_CS 7
#define SD_CS 8

// Pins for status indicators and controls
#define GREEN_LED 4
#define RED_LED 6
#define BUZZER 5
#define SERVO_PIN 3
#define BUTTON_PIN 2

// Servo control values for continuous rotation servo
const uint8_t SERVO_STOP = 90;         // Stop point (should be calibrated with potentiometer)
const uint8_t SERVO_OPEN_SPEED = 0;    // Full speed one direction
const uint8_t SERVO_CLOSE_SPEED = 180; // Full speed other direction
const uint16_t DOOR_MOVE_TIME = 360;   // Time for door to move from open to close position (360 ms)
const uint16_t DOOR_OPEN_TIME = 3000;  // Time door stays open before auto-closing (3 seconds)

// Initialize RFID, Servo, ArduCAM, SD Card and LCD objects
MFRC522 mfrc522(RFID_CS, RST_PIN);
RFIDAuth rfidAuth(SERVER_ADDRESS, SERVER_PORT, DEVICE_UUID);
Servo doorServo;
ArduCAM myCAM(OV5642, ARDUCAM_CS);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Initialize NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Custom messages for LCD
const char *MSG_READY = "Ready: Scan Card";
const char *MSG_ACCESS_GRANTED = "Access Granted!";
const char *MSG_ACCESS_DENIED = "Access Denied!";

// Door and button state variables
bool doorIsOpen = false;
unsigned long lastDoorAction = 0;
unsigned long doorOpenStartTime = 0; // Track when door was opened
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // Debounce time in milliseconds

void initializeHardware();
void setupWiFi();
void initializeRTC();
bool isDaylightSaving(int month, int day);
String getTimestampFilename();
void processRFIDCard();
void capturePhotoToSD();
void checkButton();
void openDoor();
void closeDoor();
void signalAccessGranted();
void signalAccessDenied();
void stopServo();

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  delay(2000);

  // Initialize hardware
  initializeHardware();

  // Initialize WiFi and RTC
  setupWiFi();
  initializeRTC();

  Serial.println("RFID Door Control System");
  Serial.println("Scan your card or press button to open door...");
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    setupWiFi();
  }

  // Check for RFID cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
  {
    processRFIDCard();
  }

  // Check button
  checkButton();

  // Check if current door movement is complete
  if (millis() - lastDoorAction >= DOOR_MOVE_TIME)
  {
    stopServo();
  }

  // Check if door has been open long enough and needs to auto-close
  if (doorIsOpen && (millis() - doorOpenStartTime >= DOOR_OPEN_TIME))
  {
    closeDoor();

    // After closing, show ready message on LCD
    lcd.clear();
    lcd.print(MSG_READY);
  }
}

void initializeHardware()
{
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Initialize pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Using internal pull-up resistor
  digitalWrite(ARDUCAM_CS, HIGH);

  // Initial LED states
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // Initialize SPI bus
  SPI.begin();

  // Initialize MFRC522
  mfrc522.PCD_Init();

  // Initialize ArduCAM
  uint8_t vid, pid;
  uint8_t temp;

  // Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);

  // Check SPI interface
  while (1)
  {
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55)
    {
      Serial.println(F("ArduCAM SPI interface Error!"));
      delay(1000);
    }
    else
    {
      Serial.println(F("ArduCAM SPI interface OK."));
      break;
    }
  }

  // Check for OV5642 camera module
  while (1)
  {
    myCAM.wrSensorReg16_8(0xff, 0x01);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    if ((vid != 0x56) || (pid != 0x42))
    {
      Serial.println(F("Can't find OV5642 module!"));
      delay(1000);
    }
    else
    {
      Serial.println(F("OV5642 detected."));
      break;
    }
  }

  // Initialize SD Card
  if (!SD.begin(SD_CS))
  {
    Serial.println(F("SD Card Error!"));
  }
  else
  {
    Serial.println(F("SD Card detected."));
  }

  // Configure camera settings
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK); // VSYNC is active HIGH
  myCAM.OV5642_set_JPEG_size(OV5642_320x240);      // Set resolution to 320x240

  // Initialize servo
  doorServo.attach(SERVO_PIN);
  stopServo(); // Make sure servo is stopped at startup

  // Show ready message on LCD
  lcd.clear();
  lcd.print(MSG_READY);
}

void setupWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeRTC()
{
  RTC.begin();
  timeClient.begin();
  timeClient.update();

  const int baseOffset = 2; // Lithuania base UTC+2
  RTCTime currentTime = RTCTime(timeClient.getEpochTime());
  bool isDST = isDaylightSaving(Month2int(currentTime.getMonth()), currentTime.getDayOfMonth());
  int finalOffset = isDST ? baseOffset + 1 : baseOffset;

  auto unixTime = timeClient.getEpochTime() + (finalOffset * 3600);
  RTCTime timeToSet = RTCTime(unixTime);
  RTC.setTime(timeToSet);
}

bool isDaylightSaving(int month, int day)
{
  if (month < 3 || month > 10)
    return false;
  if (month > 3 && month < 10)
    return true;

  int lastSunday = 31 - (day % 7);
  if (month == 3)
    return day >= lastSunday;
  if (month == 10)
    return day < lastSunday;
  return false;
}

String getTimestampFilename()
{
  RTCTime currentTime;
  RTC.getTime(currentTime);

  // Create a folder for the current date if it doesn't exist
  char dateFolder[16];
  sprintf(dateFolder, "/%04d%02d%02d",
          currentTime.getYear(),
          Month2int(currentTime.getMonth()),
          currentTime.getDayOfMonth());

  if (!SD.exists(dateFolder))
  {
    SD.mkdir(dateFolder);
  }

  // Create the full timestamp filename with path
  char filename[64];
  sprintf(filename, "%s/%02d%02d%02d.jpg",
          dateFolder,
          currentTime.getHour(),
          currentTime.getMinutes(),
          currentTime.getSeconds());

  return String(filename);
}

void processRFIDCard()
{
  // Show scanning message
  lcd.clear();
  lcd.print("Checking Card...");

  // Check authorization with server
  bool authorized = rfidAuth.checkCardAuthorization(mfrc522.uid);

  // Handle authorization result
  if (authorized)
  {
    lcd.clear();
    lcd.print(MSG_ACCESS_GRANTED);
    signalAccessGranted();
    if (!doorIsOpen)
    {
      // Only open if door is closed
      openDoor();
    }
  }
  else
  {
    lcd.clear();
    lcd.print(MSG_ACCESS_DENIED);
    signalAccessDenied();
  }

  // Halt PICC and stop encryption on PCD
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void capturePhotoToSD()
{
  String filename = getTimestampFilename();
  byte buf[256];

  // Prepare camera
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();

  // Start capture
  Serial.println(F("Starting Capture..."));
  myCAM.start_capture();

  // Wait for capture to complete
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
    ;
  Serial.println(F("Capture Done."));

  uint32_t length = myCAM.read_fifo_length();

  if (length >= MAX_FIFO_SIZE || length == 0)
  {
    Serial.println(F("Capture size error"));
    return;
  }

  // Open file
  File outFile = SD.open(filename, O_WRITE | O_CREAT | O_TRUNC);
  if (!outFile)
  {
    Serial.println(F("File open failed"));
    return;
  }

  // Read and save image data
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  bool is_header = false;
  int i = 0;
  uint8_t temp, temp_last = 0;

  while (length--)
  {
    temp_last = temp;
    temp = SPI.transfer(0x00);

    // Check for end of image
    if ((temp == 0xD9) && (temp_last == 0xFF))
    {
      buf[i++] = temp;
      myCAM.CS_HIGH();
      outFile.write(buf, i);
      outFile.close();
      Serial.print(F("Image saved as "));
      Serial.println(filename);
      break;
    }

    if (is_header)
    {
      if (i < 256)
      {
        buf[i++] = temp;
      }
      else
      {
        myCAM.CS_HIGH();
        outFile.write(buf, 256);
        i = 0;
        buf[i++] = temp;
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
      }
    }
    else if ((temp == 0xD8) && (temp_last == 0xFF))
    {
      is_header = true;
      buf[i++] = temp_last;
      buf[i++] = temp;
    }
  }
}

void checkButton()
{
  // Read button state
  int buttonState = digitalRead(BUTTON_PIN);

  // Check if the button state has changed
  if (buttonState != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  // If enough time has passed, check if the button state has really changed
  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    // If button is pressed (LOW) and door isn't moving
    if (buttonState == LOW && (millis() - lastDoorAction >= DOOR_MOVE_TIME))
    {
      Serial.println("Button pressed");
      if (!doorIsOpen)
      {
        // Only open if door is closed
        openDoor();
      }
    }
  }

  lastButtonState = buttonState;
}

void openDoor()
{
  Serial.println("Opening door...");

  doorServo.write(SERVO_OPEN_SPEED); // Rotate to open position
  doorIsOpen = true;
  lastDoorAction = millis();
  doorOpenStartTime = millis(); // Start timing the open duration
  digitalWrite(GREEN_LED, HIGH);
  tone(BUZZER, 2000, 200);
}

void closeDoor()
{
  Serial.println("Closing door...");

  doorServo.write(SERVO_CLOSE_SPEED); // Rotate back to closed position
  doorIsOpen = false;
  lastDoorAction = millis();
  digitalWrite(GREEN_LED, LOW);
  // tone(BUZZER, 1000, 200);
}

void stopServo()
{
  doorServo.write(SERVO_STOP); // Stop servo rotation
}

void signalAccessGranted()
{
  digitalWrite(GREEN_LED, HIGH);
  tone(BUZZER, 2000, 200);
}

void signalAccessDenied()
{
  digitalWrite(RED_LED, HIGH);

  // Capture photo of unauthorized access attempt
  capturePhotoToSD();

  for (int i = 0; i < 3; i++)
  {
    tone(BUZZER, 500, 200);
    delay(300);
  }

  digitalWrite(RED_LED, LOW);

  delay(2000);
  lcd.clear();
  lcd.print(MSG_READY);
}