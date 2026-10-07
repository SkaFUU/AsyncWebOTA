#include <AsyncWebOTA.h>

AsyncWebOTA webOTA;

// Variables for readouts: they are read by reference, so keep them global (or static)
int counter = 0;
float temperature = 23.5;
bool ledState = false;
String statusMessage = "OK";

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin("your-ssid", "your-password");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure WebOTA
  webOTA.setDeviceName("My IoT Device");

  // Add readouts
  webOTA.addReadout("Counter", counter);
  webOTA.addReadout("Temperature", temperature, "°C");
  webOTA.addReadout("LED", ledState, "On", "Off");
  webOTA.addReadout("Status", statusMessage);

  // Add buttons (the callbacks run inside the web server: keep them short, no delay())
  webOTA.addButton("toggle-led", "Toggle LED", []() {
    ledState = !ledState;
    Serial.println("LED toggled");
  });

  webOTA.addButton("restart", "Restart Device", []() {
    AsyncWebOTA::restartDevice();
  });

  // Start WebOTA server
  webOTA.begin();
}

void loop() {
  // Update variables for readouts
  counter++;
  temperature += 0.1;
  if (temperature > 30.0) temperature = 20.0;

  delay(1000);
}
