#ifndef RFIDAuth_h
#define RFIDAuth_h

#include <Arduino.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <ArduinoBearSSL.h>
#include <AES128.h>

#include "arduino_secrets.h"

class RFIDAuth
{
private:
    const char *serverAddress;
    int serverPort;
    const char *deviceUUID;
    WiFiClient client;

    uint8_t aesKey[16] = AES_KEY;

    // Generate a cryptographically secure random IV
    void generateRandomIV(uint8_t *iv)
    {
        // Seed the random number generator with a random value
        randomSeed(analogRead(A0) * analogRead(A1));

        // Fill the IV with random bytes
        for (int i = 0; i < 16; i++)
        {
            iv[i] = random(256); // Generate random byte (0-255)
        }
    }

    // Convert RFID UID to HEX format string
    String formatUID(byte *uidBytes, byte size)
    {
        String result = "";
        for (byte i = 0; i < size; i++)
        {
            if (uidBytes[i] < 0x10)
            {
                result += "0";
            }
            result += String(uidBytes[i], HEX);
        }

        result.toUpperCase();
        return result;
    }

    // Helper function to convert byte array to hex string (ASCII)
    String byteArrayToHexString(uint8_t *array, size_t size)
    {
        const char hexChars[] = "0123456789abcdef";
        String result;
        result.reserve(size * 2);

        for (size_t i = 0; i < size; i++)
        {
            uint8_t byte = array[i];
            result += hexChars[byte >> 4];
            result += hexChars[byte & 0x0F];
        }

        return result;
    }

    // Debug print function
    void printBytes(const char *label, uint8_t *data, size_t size)
    {
        Serial.print(label);
        Serial.print(" [");
        for (size_t i = 0; i < size; i++)
        {
            if (i > 0)
                Serial.print(" ");
            Serial.print(data[i], DEC);
        }
        Serial.print("] HEX: ");
        Serial.println(byteArrayToHexString(data, size));
    }

    // Encrypt the UID using AES-128-CBC with random IV
    void encryptUID(byte *uidBytes, byte size, String &encryptedContent, String &ivHex)
    {
        // Generate a new random IV
        uint8_t iv[16];
        generateRandomIV(iv);

        // Store IV hex string before encryption
        ivHex = byteArrayToHexString(iv, 16);

        // First convert UID to hex string format
        String formattedUID = formatUID(uidBytes, size);
        Serial.print("Formatted UID (hex): ");
        Serial.println(formattedUID);

        // Calculate exact number of bytes needed for the UID
        size_t uidByteLength = size;

        // Prepare input buffer with padding
        uint8_t input[16] = {0}; // AES block size

        // Copy direct UID bytes
        memcpy(input, uidBytes, uidByteLength);

        // Add PKCS7 padding
        size_t padLength = 16 - (uidByteLength % 16); // Calculate padding length
        for (size_t i = uidByteLength; i < 16; i++)
        {
            input[i] = padLength;
        }

        printBytes("Input before encryption", input, 16);
        printBytes("Random IV", iv, 16);

        // Encrypt the padded data
        AES128.runEnc(aesKey, 16, input, 16, iv);

        printBytes("Encrypted bytes", input, 16);

        // Convert encrypted data to hex string
        encryptedContent = byteArrayToHexString(input, 16);

        Serial.print("IV (hex): ");
        Serial.println(ivHex);
        Serial.print("Encrypted content (hex): ");
        Serial.println(encryptedContent);
    }

public:
    RFIDAuth(const char *server, int port, const char *uuid)
    {
        serverAddress = server;
        serverPort = port;
        deviceUUID = uuid;
    }

    bool checkCardAuthorization(MFRC522::Uid uid)
    {
        Serial.print("Attempting to connect to server: ");
        Serial.print(serverAddress);
        Serial.print(":");
        Serial.println(serverPort);

        if (!client.connect(serverAddress, serverPort))
        {
            Serial.println("Connection failed!");
            return false;
        }

        Serial.println("Connected to server successfully");

        // Encrypt the card UID and get IV separately
        String encryptedContent, ivHex;
        encryptUID(uid.uidByte, uid.size, encryptedContent, ivHex);

        // Create JSON request
        StaticJsonDocument<1024> doc;
        doc["UUID"] = deviceUUID;
        doc["iv"] = ivHex;
        doc["content"] = encryptedContent;

        String jsonString;
        serializeJson(doc, jsonString);

        Serial.print("Sending request: ");
        Serial.println(jsonString);

        // Send HTTP POST request
        client.println("POST / HTTP/1.1");
        client.print("Host: ");
        client.println(serverAddress);
        client.println("Content-Type: application/json");
        client.print("Content-Length: ");
        client.println(jsonString.length());
        client.println("Connection: close");
        client.println();
        client.println(jsonString);

        // Wait for response with timeout
        unsigned long timeout = millis();
        while (client.available() == 0)
        {
            if (millis() - timeout > 5000)
            {
                Serial.println("Request timeout!");
                client.stop();
                return false;
            }
        }

        Serial.println("Received response from server:");

        // Read response headers
        bool authorized = false;
        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            Serial.println(line);
            if (line.startsWith("HTTP/1.1"))
            {
                authorized = (line.indexOf("200") > 0);
            }
            if (line == "\r")
            {
                break;
            }
        }

        // Read response body
        String response = "";
        while (client.available())
        {
            response += (char)client.read();
        }
        Serial.print("Response body: ");

        if (authorized)
        {
            Serial.print("Access granted for user: ");
            Serial.println(response);
        }
        else
        {
            Serial.println("Access denied");
        }

        client.stop();
        return authorized;
    }
};

#endif