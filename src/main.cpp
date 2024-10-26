#include <Arduino.h>
#include <MFRC522.h>
#include <Servo.h>

#include "secrets.h"

// Pins for RFID RC522
#define RST_PIN 9
#define SS_PIN 10

// Pins for status indicators and controls
#define GREEN_LED 7
#define RED_LED 6
#define BUZZER 5
#define SERVO_PIN 3
#define BUTTON_PIN 2

// Servo control values for continuous rotation servo
#define SERVO_STOP 90         // Stop point (should be calibrated with potentiometer)
#define SERVO_OPEN_SPEED 0    // Full speed one direction
#define SERVO_CLOSE_SPEED 180 // Full speed other direction
#define DOOR_MOVE_TIME 360    // Time for door to move from open to close position (360 ms)
#define DOOR_OPEN_TIME 3000   // Time door stays open before auto-closing (3 seconds)

// MFRC522 and Servo instances
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo doorServo;

// Array of authorized UIDs
extern const byte AUTHORIZED_CARDS[][4];

const int NUM_CARDS = NUM_AUTHORIZED_CARDS;

// Door and button state variables
bool doorIsOpen = false;
unsigned long lastDoorAction = 0;
unsigned long doorOpenStartTime = 0; // Track when door was opened
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // Debounce time in milliseconds

void initializeHardware();
void processRFIDCard();
void openDoor();
void closeDoor();
bool compareUID(byte *uid1, const byte *uid2);
void signalAccessGranted();
void signalAccessDenied();
void stopServo();
void checkButton();

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize hardware
  initializeHardware();

  Serial.println("RFID Door Control System");
  Serial.println("Scan your card or press button to open door...");
}

void loop()
{
  // Check button
  checkButton();

  // Check for RFID cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
  {
    processRFIDCard();
  }

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
  // Show UID on serial monitor
  Serial.print("Card UID:");
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Check if card is authorized
  bool authorized = false;
  for (int i = 0; i < NUM_CARDS; i++)
  {
    if (compareUID(mfrc522.uid.uidByte, AUTHORIZED_CARDS[i]))
    {
      authorized = true;
      break;
    }
  }

  // Handle authorization result
  if (authorized)
  {
    Serial.println("Access Granted!");
    signalAccessGranted();
    if (!doorIsOpen)
    {
      // Only open if door is closed
      openDoor();
    }
  }
  else
  {
    Serial.println("Access Denied!");
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

bool compareUID(byte *uid1, const byte *uid2)
{
  for (byte i = 0; i < 4; i++)
  {
    if (uid1[i] != uid2[i])
    {
      return false;
    }
  }
  return true;
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