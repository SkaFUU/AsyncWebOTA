#include "AsyncWebOTA.h"
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#elif defined(ESP32)
#include <Update.h>
#include <WiFi.h>
#elif defined(ARDUINO_ARCH_RP2040)
#include <LittleFS.h>
#include <WiFi.h>
#endif

// HTML Templates stored in PROGMEM
const char htmlHeader[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>)rawliteral";

const char htmlMiddle1[] PROGMEM = R"rawliteral(</title>
  <style>
    :root {
      --primary: #3a86ff;
      --bg: #121212;
      --card-bg: #1e1e1e;
      --text: #e0e0e0;
      --border: #333;
    }
    body {
      font-family: Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text);
      margin: 0;
      padding: 10px;
      min-height: 100vh;
      box-sizing: border-box;
      -webkit-text-size-adjust: 100%;
    }
    .container {
      max-width: 100%;
      width: 95%;
      border: 2px solid var(--primary);
      border-radius: 15px;
      padding: 15px;
      margin: 10px auto;
      background-color: var(--card-bg);
    }
    @media (min-width: 768px) {
      .container {
        max-width: 400px;
        padding: 20px;
      }
    }
    h1 {
      font-size: 28px;
      text-align: center;
      color: var(--primary);
      margin: 0 0 0 0;
    }
    .section {
      margin-bottom: 25px;
      padding-bottom: 20px;
      border-bottom: 1px solid var(--border);
    }
    .section:last-child {
      border-bottom: none;
      margin-bottom: 0;
      padding-bottom: 0;
    }
    h2 {
      font-size: 20px;
      text-align: center;
      margin: 0 0 10px 0;
      color: var(--primary);
    }
    .file-input {
      width: 100%;
      padding: 10px;
      margin: 10px 0;
      border: 1px solid var(--border);
      border-radius: 5px;
      background-color: var(--bg);
      color: var(--text);
      box-sizing: border-box;
    }
    .file-update {
      width: 100%;
      padding: 12px;
      background-color: var(--primary);
      color: white;
      border: none;
      border-radius: 5px;
      font-size: 16px;
      cursor: pointer;
      margin-top: 10px;
    }
  </style>
  </head>
  <body>
    <div class="container">
      <div class="section">
        <h1>)rawliteral";

const char htmlMiddle2[] PROGMEM = R"rawliteral(</h1>
          </div>)rawliteral";

const char htmlFooter[] PROGMEM = R"rawliteral(
          <div class="section">
            <h2>Firmware Update</h2>
              <form method='POST' action='/update' enctype='multipart/form-data'>
                <input type='file' accept=".bin" name='update' class="file-input">
                <input type='submit' value='Update Firmware' class="file-update">
              </form>
          </div>
        </div>
      </body>
      </html>)rawliteral";

const char htmlReadoutHeader[] PROGMEM = R"rawliteral(
    <style>
      .readout-container {
        background-color: var(--bg);
        border: 1px solid var(--border);
        border-radius: 5px;
        padding: 15px;
        margin-top: 10px;
        text-align: center;
      }
      #readout {
        font-size: 18px;
        color: var(--primary);
        margin: 0;
        font-family: monospace;
        word-break: break-all;
      }
    </style>   
    <div class="section">
      <h2>Readout</h2>
      <div class="readout-container" id="readout-container">
)rawliteral";

const char htmlReadoutMiddle[] PROGMEM = R"rawliteral(
  </div>
    </div>
    <script>
      async function updateReadout() {
        try {
          const response = await fetch('/readout');
          const data = await response.json();
)rawliteral";

const char htmlReadoutFooter[] PROGMEM = R"rawliteral(
        } catch(e) {
          console.error('Readout error:', e);
        }
      }
      setInterval(updateReadout, 1000);
      updateReadout();
    </script>
)rawliteral";

const char htmlButtons[] PROGMEM = R"rawliteral(
      <style>
        .buttons {
          flex: 1 1 calc(50% - 10px);
          min-width: 120px;
          padding: 12px;
          background-color: var(--primary);
          color: white;
          border: none;
          border-radius: 5px;
          font-size: 16px;
          cursor: pointer;
          text-align: center;
        }
        .actions-container {
          display: flex;
          flex-wrap: wrap;
          gap: 10px;
          margin-top: 10px;
        }
      </style>
      <div class="section">
        <h2>Actions</h2>
        <div class="actions-container">
)rawliteral";

AsyncWebOTA::AsyncWebOTA() : server(80) {}

void AsyncWebOTA::setDeviceName(String name)
{
  deviceName = std::move(name);
}

bool AsyncWebOTA::begin()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WebOTA] Error: WiFi not connected");
    return false;
  }

  setupServer();
  server.begin();
  Serial.print("[WebOTA] Server ready at http://");
  Serial.println(WiFi.localIP());
  return true;
}

void AsyncWebOTA::end()
{
  server.end();
  Serial.println("[WebOTA] Server stopped");
}

void AsyncWebOTA::addReadout(const String &label, const String &variable)
{
  readoutItems.push_back({label, [&variable]()
                          { return variable; }});
  updateReadoutSection();
}

void AsyncWebOTA::addReadout(const String &label, const bool &variable,
                             const String &trueText, const String &falseText)
{
  readoutItems.push_back({label, [&variable, trueText, falseText]()
                          { return variable ? trueText : falseText; }});
  updateReadoutSection();
}

void AsyncWebOTA::addReadout(const String &label, const int &variable, const String &unit)
{
  readoutItems.push_back({label, [&variable, unit]()
                          { return String(variable) + unit; }});
  updateReadoutSection();
}

void AsyncWebOTA::addReadout(const String &label, const float &variable, const String &unit)
{
  readoutItems.push_back({label, [&variable, unit]()
                          { return String(variable, 1) + unit; }});
  updateReadoutSection();
}

void AsyncWebOTA::updateReadoutSection()
{
  // HTML-Abschnitt wie zuvor
  readoutSection = FPSTR(htmlReadoutHeader);
  readoutSection.reserve(512 + (readoutItems.size() * 96));

  for (size_t i = 0; i < readoutItems.size(); i++)
  {
    readoutSection += "<div class=\"readout\" id=\"readout\">";
    readoutSection += "<span>" + readoutItems[i].label + ": </span>";
    readoutSection += "<span id=\"readout-" + String(i) + "\">-</span>";
    readoutSection += "</div>";
  }

  readoutSection += FPSTR(htmlReadoutMiddle);

  for (size_t i = 0; i < readoutItems.size(); i++)
  {
    readoutSection += "document.getElementById('readout-" + String(i) + "').innerText = data.";
    readoutSection += readoutItems[i].label;
    readoutSection += ";";
  }

  readoutSection += FPSTR(htmlReadoutFooter);

  server.on("/readout", HTTP_GET, [this](AsyncWebServerRequest *request)
            {
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      StaticJsonDocument<256> doc;  // Anpassen je nach Datenmenge!
      JsonObject root = doc.to<JsonObject>();
      for (size_t i = 0; i < readoutItems.size(); i++) {
          root[readoutItems[i].label] = readoutItems[i].valueGetter();
      }
      serializeJson(root, *response);
      request->send(response); });
}

void AsyncWebOTA::addButton(const String &id, const String &label, std::function<void()> onClick)
{
  server.on(("/" + id).c_str(), HTTP_GET, [onClick, id](AsyncWebServerRequest *request)
            {
    if (onClick) onClick();
    request->send(200, "text/plain", id + " pressed"); });

  if (buttonsSection.isEmpty())
  {
    buttonsSection = FPSTR(htmlButtons);
  }

  buttonsSection += String(R"(<button id=")") + id + R"(" class="buttons">)" + label + R"(</button>)";
  buttonIds.push_back(id);
}

void AsyncWebOTA::setupServer()
{
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
            {
    String html;
    html.reserve(2048);
    html += FPSTR(htmlHeader);
    html += deviceName;
    html += FPSTR(htmlMiddle1);
    html += deviceName;
    html += FPSTR(htmlMiddle2);

    if (!readoutSection.isEmpty()) {
      html += readoutSection;
    }

    if (!buttonsSection.isEmpty()) {
      html += buttonsSection + "</div></div>";
      
      html += R"(<script>)";
      for (const auto& id : buttonIds) {
        html += String(R"(document.getElementById(")") + id + R"(").onclick = function() { fetch('/)" + id + R"('); };)";
      }
      html += R"(</script>)";
    }

    html += FPSTR(htmlFooter);
    request->send(200, "text/html", html); });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    request->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    AsyncWebOTA::restartDevice(); }, handleUpdate);
}

void AsyncWebOTA::handleUpdate(AsyncWebServerRequest *request, String filename,
                               size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    Serial.printf("[WebOTA] Start update: %s\n", filename.c_str());
    Serial.setDebugOutput(true);

#if defined(ESP8266)
    Update.runAsync(true);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace, U_FLASH))
    {
      Update.printError(Serial);
    }
#elif defined(ARDUINO_ARCH_RP2040)
    WiFiUDP::stopAll();
    FSInfo i;
    LittleFS.begin();
    LittleFS.info(i);
    uint32_t maxSketchSpace = i.totalBytes - i.usedBytes;
    if (!Update.begin(maxSketchSpace))
    {
      Update.printError(Serial);
    }
#elif defined(ESP32)
    if (!Update.begin())
    {
      Update.printError(Serial);
    }
#endif
  }

  if (Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }

  if (final)
  {
    if (Update.end(true))
    {
      Serial.printf("[WebOTA] Update Success: %u bytes\n", index + len);
    }
    else
    {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  }
}

void AsyncWebOTA::restartDevice()
{
  Serial.println("[WebOTA] Restarting device...");
  delay(100);
#if defined(ARDUINO_ARCH_RP2040)
  rp2040.restart();
#else
  ESP.restart();
#endif
}