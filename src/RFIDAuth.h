#ifndef RFIDAuth_h
#define RFIDAuth_h

#include <Arduino.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <ArduinoBearSSL.h> // This library is required for AES128
#include <AES128.h>

// Include SCE5 headers for hardware RNG
#ifdef __cplusplus
extern "C"
{
#endif
#include <hw_sce_private.h>
#include <hw_sce_trng_private.h>
#ifdef __cplusplus
}
#endif

#include "arduino_secrets.h"

class RFIDAuth
{
private:
    static const size_t AES_BLOCK_SIZE = 16;
    static const unsigned long REQUEST_TIMEOUT_MS = 5000;
    static const size_t JSON_BUFFER_SIZE = 180;

    const char *serverAddress;
    int serverPort;
    const char *deviceUUID;
    WiFiClient client;
    uint8_t aesKey[AES_BLOCK_SIZE] = AES_KEY;
    bool sce5Initialized = false;

    // Initialize the SCE5 module for secure random number generation
    bool initializeSCE5()
    {
        if (sce5Initialized)
            return true;

        HW_SCE_PowerOn();
        fsp_err_t err = HW_SCE_McuSpecificInit();
        if (err != FSP_SUCCESS)
        {
            Serial.println("Failed to initialize SCE5!");
            return false;
        }

        sce5Initialized = true;
        return true;
    }

    // Generate a cryptographically secure random IV using hardware TRNG
    bool generateSecureRandomIV(uint8_t *iv)
    {
        if (!initializeSCE5())
        {
            return false;
        }

        // SCE5 generates 128-bit (16-byte) random numbers
        uint32_t random_data[4] = {0}; // 4 x 32-bit = 128-bit
        fsp_err_t err = HW_SCE_RNG_Read(random_data);

        if (err != FSP_SUCCESS)
        {
            Serial.println("Failed to generate random IV!");
            return false;
        }

        // Copy the random data to the IV buffer
        memcpy(iv, random_data, AES_BLOCK_SIZE);
        return true;
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
    bool encryptUID(byte *uidBytes, byte size, String &encryptedContent, String &ivHex)
    {
        // Generate a new random IV
        uint8_t iv[AES_BLOCK_SIZE];
        if (!generateSecureRandomIV(iv))
        {
            Serial.println("Failed to generate secure IV!");
            return false;
        }

        // Store IV hex string before encryption
        ivHex = byteArrayToHexString(iv, AES_BLOCK_SIZE);

        // First convert UID to hex string format
        String formattedUID = formatUID(uidBytes, size);
        Serial.print("Formatted UID (hex): ");
        Serial.println(formattedUID);

        // Calculate exact number of bytes needed for the UID
        size_t uidByteLength = size;

        // Prepare input buffer with padding
        uint8_t input[AES_BLOCK_SIZE] = {0}; // AES block size

        // Copy direct UID bytes
        memcpy(input, uidBytes, uidByteLength);

        // Add PKCS7 padding
        size_t padLength = AES_BLOCK_SIZE - (uidByteLength % AES_BLOCK_SIZE); // Calculate padding length
        for (size_t i = uidByteLength; i < AES_BLOCK_SIZE; i++)
        {
            input[i] = padLength;
        }

        printBytes("Input before encryption", input, AES_BLOCK_SIZE);
        printBytes("Random IV", iv, AES_BLOCK_SIZE);

        // Encrypt the padded data
        AES128.runEnc(aesKey, AES_BLOCK_SIZE, input, AES_BLOCK_SIZE, iv);

        printBytes("Encrypted bytes", input, AES_BLOCK_SIZE);

        // Convert encrypted data to hex string
        encryptedContent = byteArrayToHexString(input, AES_BLOCK_SIZE);

        Serial.print("IV (hex): ");
        Serial.println(ivHex);
        Serial.print("Encrypted content (hex): ");
        Serial.println(encryptedContent);

        return true;
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
        if (!encryptUID(uid.uidByte, uid.size, encryptedContent, ivHex))
        {
            Serial.println("Encryption failed!");
            client.stop();
            return false;
        }

        // Create JSON request
        StaticJsonDocument<JSON_BUFFER_SIZE> doc;
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
            if (millis() - timeout > REQUEST_TIMEOUT_MS)
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