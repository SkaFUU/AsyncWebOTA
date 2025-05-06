#ifndef ASYNC_WEB_OTA_H
#define ASYNC_WEB_OTA_H

#include <ESPAsyncWebServer.h>
#include <functional>
#include <vector>

class AsyncWebOTA
{
private:
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
    String readoutSection;
    String buttonsSection;
    std::vector<String> buttonIds;

    struct ReadoutItem
    {
        String label;
        std::function<String(void)> valueGetter;
    };
    std::vector<ReadoutItem> readoutItems;

    void setupServer();
    void updateReadoutSection();
    static void handleUpdate(AsyncWebServerRequest *request, String filename,
                             size_t index, uint8_t *data, size_t len, bool final);
    static String getPlatformUpdateError();

public:
    AsyncWebOTA();
    bool begin();
    void end();
    void setDeviceName(String name);
    void addButton(const String &id, const String &label,
                   std::function<void()> onClick = nullptr);
    static void restartDevice();

    void addReadout(const String &label, const String &variable);
    void addReadout(const String &label, const bool &variable,const String &trueText = "On", const String &falseText = "Off");
    void addReadout(const String &label, const int &variable, const String &unit = "");
    void addReadout(const String &label, const float &variable, const String &unit = "");
};

#endif