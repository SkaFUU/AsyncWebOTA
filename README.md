# AsyncWebOTA

AsyncWebOTA provides a web-based OTA (Over-The-Air) update interface with real-time readouts and customizable buttons for ESP8266, ESP32, and RP2040 devices.

## Features

- Web-based OTA firmware updates
- Real-time variable readouts
- Customizable action buttons
- Dark mode UI
- Support for multiple platforms (ESP8266, ESP32, RP2040)

## Installation

1. Download the latest release
2. In Arduino IDE: Sketch > Include Library > Add .ZIP Library
3. Select the downloaded file

## Basic Usage

```cpp
#include <AsyncWebOTA.h>

AsyncWebOTA webOTA;

void setup() {
  Serial.begin(115200);
  WiFi.begin("SSID", "password");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  webOTA.setDeviceName("My Device");
  
  // Add readouts
  int sensorValue = 42;
  webOTA.addReadout("Temperature", sensorValue, "°C");
  
  // Add buttons
  webOTA.addButton("restart", "Restart", []() {
    Serial.println("Restart requested");
    delay(1000);
    AsyncWebOTA::restartDevice();
  });

  webOTA.begin();
}

void loop() {
  // Your code here
}