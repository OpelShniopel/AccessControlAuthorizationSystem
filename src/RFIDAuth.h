#ifndef RFIDAuth_h
#define RFIDAuth_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiS3.h>
#include <MFRC522.h>

class RFIDAuth
{
private:
    const char *serverAddress;
    int serverPort;
    const char *deviceUUID;
    WiFiClient client;

    String formatUID(byte *uidBytes, byte size)
    {
        String uidString = "";
        for (byte i = 0; i < size; i++)
        {
            if (uidBytes[i] < 0x10)
            {
                uidString += "0";
            }
            uidString += String(uidBytes[i], HEX);
        }
        uidString.toLowerCase();
        return uidString;
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
            Serial.println("Connection failed! Error details:");
            Serial.print("Connection state: ");
            Serial.println(client.connected() ? "Connected" : "Not connected");
            return false;
        }

        Serial.println("Connected to server successfully");

        // Format card UID (now without spaces)
        String cardUID = formatUID(uid.uidByte, uid.size);
        Serial.print("Card UID (hex): ");
        Serial.println(cardUID);

        // Create JSON request
        StaticJsonDocument<1024> doc;
        doc["UUID"] = deviceUUID;
        doc["content"] = cardUID;

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
            { // End of headers
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
        Serial.println(response);

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