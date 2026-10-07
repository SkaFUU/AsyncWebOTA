#ifndef ASYNC_WEB_OTA_H
#define ASYNC_WEB_OTA_H

#include <Arduino.h>

// Included here so sketches can use WiFi without including it themselves.
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
#include <WiFi.h>
#endif

#include <ESPAsyncWebServer.h>
#include <functional>
#include <vector>

class AsyncWebOTA
{
public:
    AsyncWebOTA();

    // Starts the web server on port 80. WiFi must already be connected.
    bool begin();
    void end();

    void setDeviceName(String name);

    // Adds a button to the page. onClick runs inside the web server callback:
    // keep it short and do not call delay() in it.
    void addButton(const String &id, const String &label,
                   std::function<void()> onClick = nullptr);

    // Shows a variable on the page, refreshed every second. The variable is read by
    // reference, so it must stay alive (global or static, not a local in setup()).
    void addReadout(const String &label, const String &variable);
    void addReadout(const String &label, const bool &variable, const String &trueText = "On", const String &falseText = "Off");
    void addReadout(const String &label, const int &variable, const String &unit = "");
    void addReadout(const String &label, const float &variable, const String &unit = "");

    // Restarts the device shortly after the current web request has been answered.
    // Safe to call from button callbacks.
    static void restartDevice();

private:
    struct ReadoutItem
    {
        String label;
        std::function<String(void)> valueGetter;
    };
    struct Button
    {
        String id;
        String label;
    };

    AsyncWebServer server;
#if defined(ESP8266)
    String deviceName = "ESP8266";
#elif defined(ESP32)
    String deviceName = "ESP32";
#elif defined(ARDUINO_ARCH_RP2040)
    String deviceName = "RP2040";
#else
    String deviceName = "MyDevice";
#endif
    std::vector<ReadoutItem> readoutItems;
    std::vector<Button> buttons;

    void setupServer();
    String buildPage() const;
    String buildReadoutJson() const;
    static void handleUpdate(AsyncWebServerRequest *request, const String &filename,
                             size_t index, uint8_t *data, size_t len, bool final);
};

#endif
