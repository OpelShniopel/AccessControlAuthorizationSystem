#include <Arduino.h>
#include <MFRC522.h>
#include <Servo.h>
#include <WiFiS3.h>

#include "arduino_secrets.h"
#include "RFIDAuth.h"

// Pins for RFID RC522
const uint8_t RST_PIN = 9;
const uint8_t SS_PIN = 10;

// Pins for status indicators and controls
const uint8_t GREEN_LED = 4;
const uint8_t RED_LED = 6;
const uint8_t BUZZER = 5;
const uint8_t SERVO_PIN = 3;
const uint8_t BUTTON_PIN = 2;

// Servo control values for continuous rotation servo
const uint8_t SERVO_STOP = 90;         // Stop point (should be calibrated with potentiometer)
const uint8_t SERVO_OPEN_SPEED = 0;    // Full speed one direction
const uint8_t SERVO_CLOSE_SPEED = 180; // Full speed other direction
const uint16_t DOOR_MOVE_TIME = 360;   // Time for door to move from open to close position (360 ms)
const uint16_t DOOR_OPEN_TIME = 3000;  // Time door stays open before auto-closing (3 seconds)

// MFRC522 and Servo instances
MFRC522 mfrc522(SS_PIN, RST_PIN);
RFIDAuth rfidAuth(SERVER_ADDRESS, SERVER_PORT, DEVICE_UUID);
Servo doorServo;

// Door and button state variables
bool doorIsOpen = false;
unsigned long lastDoorAction = 0;
unsigned long doorOpenStartTime = 0; // Track when door was opened
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // Debounce time in milliseconds

void initializeHardware();
void setupWiFi();
void processRFIDCard();
void openDoor();
void closeDoor();
void signalAccessGranted();
void signalAccessDenied();
void stopServo();
void checkButton();

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  delay(2000);

  // Initialize hardware
  initializeHardware();
  setupWiFi();

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
  }
}

void initializeHardware()
{
  // Initialize pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Using internal pull-up resistor

  // Initial LED states
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // Initialize SPI bus
  SPI.begin();

  // Initialize MFRC522
  mfrc522.PCD_Init();

  // Initialize servo
  doorServo.attach(SERVO_PIN);
  stopServo(); // Make sure servo is stopped at startup
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

void processRFIDCard()
{
  // Check authorization with server
  bool authorized = rfidAuth.checkCardAuthorization(mfrc522.uid);

  // Handle authorization result
  if (authorized)
  {
    signalAccessGranted();
    if (!doorIsOpen)
    {
      // Only open if door is closed
      openDoor();
    }
  }
  else
  {
    signalAccessDenied();
  }

  // Halt PICC and stop encryption on PCD
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
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
  for (int i = 0; i < 3; i++)
  {
    tone(BUZZER, 500, 200);
    delay(300);
  }
  digitalWrite(RED_LED, LOW);
}