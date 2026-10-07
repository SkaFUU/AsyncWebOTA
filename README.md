# AsyncWebOTA

AsyncWebOTA provides a web-based OTA (Over-The-Air) update interface with real-time readouts and customizable buttons for ESP8266, ESP32, and RP2040 devices.

## Features

- Web-based OTA firmware updates (upload a `.bin` file from the browser)
- Real-time variable readouts, refreshed every second
- Customizable action buttons
- Dark mode UI that works on phones
- Supports ESP8266, ESP32 and RP2040 (Pico W)

## Dependencies

- [ESP Async WebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
- plus the matching TCP library for your board: [ESPAsyncTCP](https://github.com/ESP32Async/ESPAsyncTCP) (ESP8266), [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (ESP32) or [RPAsyncTCP](https://github.com/ayushsharma82/RPAsyncTCP) (RP2040)

## Installation

1. Download the latest release as ZIP
2. In the Arduino IDE: Sketch > Include Library > Add .ZIP Library
3. Select the downloaded file

## Basic Usage

```cpp
#include <AsyncWebOTA.h>

AsyncWebOTA webOTA;

int sensorValue = 42;   // readout variables must stay alive: global or static

void setup() {
  Serial.begin(115200);
  WiFi.begin("SSID", "password");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  webOTA.setDeviceName("My Device");

  // Add readouts
  webOTA.addReadout("Temperature", sensorValue, "°C");

  // Add buttons
  webOTA.addButton("restart", "Restart", []() {
    Serial.println("Restart requested");
    AsyncWebOTA::restartDevice();
  });

  webOTA.begin();
}

void loop() {
  // Your code here
}
```

Then open `http://<device-ip>/` in a browser.

## API

| Function | Description |
|---|---|
| `begin()` | Starts the web server on port 80. WiFi must be connected; returns `false` otherwise. |
| `end()` | Stops the web server. |
| `setDeviceName(name)` | Title shown on the page. |
| `addReadout(label, variable)` | Shows a `String`, `int`, `float` or `bool` variable. Optional unit (`int`, `float`) or on/off texts (`bool`). |
| `addButton(id, label, onClick)` | Adds a button; `onClick` is called when it is pressed. |
| `AsyncWebOTA::restartDevice()` | Restarts the device shortly after the current web request has been answered. |

## Notes

- **Readout variables are read by reference.** Use global or `static` variables. A local variable in `setup()` no longer exists after `setup()` returns and shows garbage or crashes the device.
- **Button callbacks run inside the web server.** Keep them short and don't call `delay()` in them. On the ESP8266 this crashes with an exception. To restart from a button, call `AsyncWebOTA::restartDevice()`, which is safe there.
- On the ESP8266 the restart runs from `loop()`, so `loop()` must keep returning.
- The OTA page has no password. Only use it in networks you trust.

## Changelog

### 1.1.0
- Fixed: ESP8266 crashed with Exception (9) when restarting after an update or from a button (#1). Thanks to @EtlamGit for the analysis and the fix in #4.
- Fixed: the example and the README code did not compile for ESP8266 and ESP32 (`WiFi` was not declared).
- Fixed: readouts with spaces or special characters in the label did not update.
- Fixed: the device restarted even when the update had failed; the page now shows whether the update worked.
- Fixed: README example used a local variable for a readout and `delay()` in a button callback. Thanks to @EtlamGit (#2, #3).
- ArduinoJson is no longer needed.

### 1.0.0
- First release.
