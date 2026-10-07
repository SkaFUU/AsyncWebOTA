#include "AsyncWebOTA.h"

#if defined(ESP8266)
#include <ESPAsyncTCP.h>
#include <Schedule.h>
#elif defined(ESP32)
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(ARDUINO_ARCH_RP2040)
#include <LittleFS.h>
#include <pico/time.h>
#endif

// HTML templates stored in PROGMEM
const char htmlHeader[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  :root { --primary: #3a86ff; --bg: #121212; --card-bg: #1e1e1e; --text: #e0e0e0; --border: #333; }
  body { font-family: Arial, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 10px;
         min-height: 100vh; box-sizing: border-box; -webkit-text-size-adjust: 100%; }
  .container { max-width: 100%; width: 95%; border: 2px solid var(--primary); border-radius: 15px; padding: 15px;
               margin: 10px auto; background-color: var(--card-bg); box-sizing: border-box; }
  @media (min-width: 768px) { .container { max-width: 400px; padding: 20px; } }
  h1 { font-size: 28px; text-align: center; color: var(--primary); margin: 0; }
  h2 { font-size: 20px; text-align: center; margin: 0 0 10px 0; color: var(--primary); }
  .section { margin-bottom: 25px; padding-bottom: 20px; border-bottom: 1px solid var(--border); }
  .section:last-child { border-bottom: none; margin-bottom: 0; padding-bottom: 0; }
  .readout-container { background-color: var(--bg); border: 1px solid var(--border); border-radius: 5px; padding: 15px;
                       margin-top: 10px; text-align: center; }
  .readout { font-size: 18px; color: var(--primary); margin: 0; font-family: monospace; word-break: break-all; }
  .actions-container { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 10px; }
  .file-input { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid var(--border); border-radius: 5px;
                background-color: var(--bg); color: var(--text); box-sizing: border-box; }
  .buttons, .file-update { padding: 12px; background-color: var(--primary); color: white; border: none; border-radius: 5px;
                           font-size: 16px; cursor: pointer; text-align: center; }
  .buttons { flex: 1 1 calc(50% - 10px); min-width: 120px; }
  .file-update { width: 100%; margin-top: 10px; }
  a { color: var(--primary); }
</style>
<title>)rawliteral";

const char htmlUpdateForm[] PROGMEM = R"rawliteral(
<div class="section">
  <h2>Firmware Update</h2>
  <form method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" accept=".bin,.bin.gz" name="update" class="file-input" required>
    <input type="submit" value="Update Firmware" class="file-update">
  </form>
</div>
</div></body></html>)rawliteral";

const char htmlReadoutScript[] PROGMEM = R"rawliteral(
<script>
  async function updateReadout() {
    try {
      const data = await (await fetch('/readout')).json();
      document.querySelectorAll('[data-readout]').forEach(el => {
        const v = data[el.dataset.readout];
        el.textContent = v === undefined ? '-' : v;
      });
    } catch (e) {
      console.error('Readout error:', e);
    }
  }
  setInterval(updateReadout, 1000);
  updateReadout();
</script>)rawliteral";

const char htmlButtonScript[] PROGMEM = R"rawliteral(
<script>
  document.querySelectorAll('[data-action]').forEach(b => b.onclick = () => fetch('/' + encodeURIComponent(b.dataset.action)));
</script>)rawliteral";

namespace
{
  // Result of the last firmware upload (the upload handler and the response handler are separate callbacks)
  bool updateRunning = false;
  bool updateSuccess = false;

  String htmlEscape(const String &s)
  {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++)
    {
      const char c = s[i];
      if (c == '&') out += F("&amp;");
      else if (c == '<') out += F("&lt;");
      else if (c == '>') out += F("&gt;");
      else if (c == '"') out += F("&quot;");
      else if (c == '\'') out += F("&#39;");
      else out += c;
    }
    return out;
  }

  // JSON string literal including quotes; '<' is escaped so it is also safe inside <script>
  String jsonString(const String &s)
  {
    String out = "\"";
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++)
    {
      const char c = s[i];
      if (c == '"' || c == '\\') { out += '\\'; out += c; }
      else if (c == '\n') out += F("\\n");
      else if (c == '\r') out += F("\\r");
      else if (c == '\t') out += F("\\t");
      else if (c == '<') out += F("\\u003c");
      else if (static_cast<unsigned char>(c) < 0x20) { char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
      else out += c;
    }
    out += '"';
    return out;
  }

  String resultPage(const String &title, const String &text, bool reload)
  {
    String html = FPSTR(htmlHeader);
    html += title;
    html += F("</title>");
    if (reload) html += F("<meta http-equiv=\"refresh\" content=\"15;url=/\">");
    html += F("</head><body><div class=\"container\"><div class=\"section\"><h1>");
    html += title;
    html += F("</h1></div><p style=\"text-align:center\">");
    html += text;
    html += F("</p><p style=\"text-align:center\"><a href=\"/\">Back</a></p></div></body></html>");
    return html;
  }
}

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
}

void AsyncWebOTA::addReadout(const String &label, const bool &variable,
                             const String &trueText, const String &falseText)
{
  readoutItems.push_back({label, [&variable, trueText, falseText]()
                          { return variable ? trueText : falseText; }});
}

void AsyncWebOTA::addReadout(const String &label, const int &variable, const String &unit)
{
  readoutItems.push_back({label, [&variable, unit]()
                          { return String(variable) + unit; }});
}

void AsyncWebOTA::addReadout(const String &label, const float &variable, const String &unit)
{
  readoutItems.push_back({label, [&variable, unit]()
                          { return String(variable, 1) + unit; }});
}

void AsyncWebOTA::addButton(const String &id, const String &label, std::function<void()> onClick)
{
  server.on(("/" + id).c_str(), HTTP_GET, [onClick, id](AsyncWebServerRequest *request)
            {
    if (onClick) onClick();
    request->send(200, "text/plain", id + " pressed"); });

  buttons.push_back({id, label});
}

String AsyncWebOTA::buildPage() const
{
  const String name = htmlEscape(deviceName);
  String html;
  html.reserve(4096 + readoutItems.size() * 96 + buttons.size() * 96);
  html += FPSTR(htmlHeader);
  html += name;
  html += F("</title></head><body><div class=\"container\"><div class=\"section\"><h1>");
  html += name;
  html += F("</h1></div>");

  if (!readoutItems.empty())
  {
    html += F("<div class=\"section\"><h2>Readout</h2><div class=\"readout-container\">");
    for (const auto &item : readoutItems)
    {
      const String label = htmlEscape(item.label);
      html += F("<div class=\"readout\"><span>");
      html += label;
      html += F(": </span><span data-readout=\"");
      html += label;
      html += F("\">-</span></div>");
    }
    html += F("</div></div>");
    html += FPSTR(htmlReadoutScript);
  }

  if (!buttons.empty())
  {
    html += F("<div class=\"section\"><h2>Actions</h2><div class=\"actions-container\">");
    for (const auto &b : buttons)
    {
      html += F("<button class=\"buttons\" data-action=\"");
      html += htmlEscape(b.id);
      html += F("\">");
      html += htmlEscape(b.label);
      html += F("</button>");
    }
    html += F("</div></div>");
    html += FPSTR(htmlButtonScript);
  }

  html += FPSTR(htmlUpdateForm);
  return html;
}

String AsyncWebOTA::buildReadoutJson() const
{
  String json = "{";
  for (size_t i = 0; i < readoutItems.size(); i++)
  {
    if (i) json += ',';
    json += jsonString(readoutItems[i].label);
    json += ':';
    json += jsonString(readoutItems[i].valueGetter());
  }
  json += '}';
  return json;
}

void AsyncWebOTA::setupServer()
{
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
            { request->send(200, "text/html", buildPage()); });

  server.on("/readout", HTTP_GET, [this](AsyncWebServerRequest *request)
            { request->send(200, "application/json", buildReadoutJson()); });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (updateSuccess) {
      request->send(200, "text/html", resultPage(F("Update successful"),
        F("The device is restarting. This page reloads in 15 seconds."), true));
      AsyncWebOTA::restartDevice();
    } else {
      request->send(500, "text/html", resultPage(F("Update failed"),
        F("The firmware was not installed. Details are printed on the serial monitor."), false));
    } }, handleUpdate);
}

void AsyncWebOTA::handleUpdate(AsyncWebServerRequest *request, const String &filename,
                               size_t index, uint8_t *data, size_t len, bool final)
{
  (void)request;

  if (index == 0)
  {
    Serial.printf("[WebOTA] Start update: %s\n", filename.c_str());
    updateSuccess = false;

#if defined(ESP8266)
    Update.runAsync(true);
    const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    updateRunning = Update.begin(maxSketchSpace, U_FLASH);
#elif defined(ARDUINO_ARCH_RP2040)
    WiFiUDP::stopAll();
    FSInfo info;
    LittleFS.begin();
    LittleFS.info(info);
    updateRunning = Update.begin(info.totalBytes - info.usedBytes);
#elif defined(ESP32)
    updateRunning = Update.begin();
#else
    updateRunning = false;
#endif

    if (!updateRunning)
    {
      Update.printError(Serial);
    }
  }

  if (!updateRunning)
  {
    return;
  }

  if (len && Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }

  if (final)
  {
    updateRunning = false;
    updateSuccess = Update.end(true);
    if (updateSuccess)
    {
      Serial.printf("[WebOTA] Update success: %u bytes\n", static_cast<unsigned>(index + len));
    }
    else
    {
      Update.printError(Serial);
    }
  }
}

void AsyncWebOTA::restartDevice()
{
  Serial.println("[WebOTA] Restarting device...");
#if defined(ESP8266)
  // Web server callbacks run in the system context, where delay() and ESP.restart() crash
  // with Exception (9) (issue #1). schedule_function runs the restart from loop() instead.
  schedule_function([]()
                    {
    delay(100);
    WiFi.disconnect(true);
    ESP.restart(); });
#elif defined(ESP32)
  // Restart from a separate task, so the web server can still send its answer first.
  xTaskCreate([](void *)
              {
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP.restart(); },
              "webota_restart", 2048, nullptr, 1, nullptr);
#elif defined(ARDUINO_ARCH_RP2040)
  // Same idea on the RP2040: restart from a timer a moment later.
  add_alarm_in_ms(300, [](alarm_id_t, void *) -> int64_t
                  {
    rp2040.restart();
    return 0; },
                  nullptr, true);
#endif
}
