// COM3505 IoT Assignment
// ESP32-S3 Feather + NTC Thermistor + 3 LED Patterns + Flask Web Dashboard
//
// This program reads an NTC thermistor, sends the readings to a Flask server,
// and polls the Flask server for the LED pattern selected in the browser UI.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>
#include <ArduinoJson.h>

// -----------------------------------------------------------------------------
// User configuration
// -----------------------------------------------------------------------------
// Before uploading this sketch, change these three values for your own network.
// 1. WIFI_SSID: your WiFi network name.
// 2. WIFI_PASSWORD: your WiFi password. Leave it as "" for an open network.
// 3. FLASK_SERVER_BASE_URL: the IP address of the laptop/PC running app.py.
//
// Example:
//   #define WIFI_SSID "MyPhoneHotspot"
//   #define WIFI_PASSWORD "my-password"
//   #define FLASK_SERVER_BASE_URL "http://192.168.1.23:5001"
//
// The ESP32 and the Flask server must be connected to the same WiFi network.
#define WIFI_SSID "ENTER_YOUR_WIFI_SSID_HERE"
#define WIFI_PASSWORD "ENTER_YOUR_WIFI_PASSWORD_HERE"
#define FLASK_SERVER_BASE_URL "http://ENTER_YOUR_WIFI_IP_HERE:5001"

// -----------------------------------------------------------------------------
// Pin configuration
// -----------------------------------------------------------------------------
// Adafruit ESP32-S3 Feather pin mapping used in this project:
// A2 = GPIO16, D5 = GPIO5, D6 = GPIO6, D9 = GPIO9.
// The thermistor voltage divider is connected to A2.
const int thermistorPin = A2;

const int redLedPin = 5;        // D5
const int yellowLedPin = 6;     // D6
const int greenLedPin = 9;      // D9

const int ledPins[] = { redLedPin, yellowLedPin, greenLedPin };
const int ledCount = sizeof(ledPins) / sizeof(ledPins[0]);

// -----------------------------------------------------------------------------
// Thermistor configuration
// -----------------------------------------------------------------------------
// These values should match the physical thermistor circuit.
// This version assumes a 180 ohm fixed resistor and a thermistor calibrated
// against 180 ohm at 25 C, matching the working assignment setup.
const float referenceVoltage = 3.3;
const float referenceResistor = 180.0;
const float beta = 3950.0;
const float nominalTemperature = 25.0;
const float nominalResistance = 180.0;

#define ADC_MAX 4095.0

// -----------------------------------------------------------------------------
// Timing configuration
// -----------------------------------------------------------------------------
// These intervals control how often the ESP32 communicates with the Flask server.
// millis() is used instead of long delay() calls so the LED patterns stay active.
const unsigned long sensorPostIntervalMs = 2000;
const unsigned long patternGetIntervalMs = 2000;
const unsigned long wifiReconnectIntervalMs = 5000;

unsigned long lastSensorPostMs = 0;
unsigned long lastPatternGetMs = 0;
unsigned long lastWifiReconnectMs = 0;
unsigned long lastPatternUpdateMs = 0;

// -----------------------------------------------------------------------------
// Runtime state
// -----------------------------------------------------------------------------
String currentPattern = "solid";

float latestTemperatureC = NAN;
int latestAdc = 0;
float latestVoltage = 0.0;
float latestResistance = NAN;
bool latestReadingValid = false;

// -----------------------------------------------------------------------------
// WiFi connection helpers
// -----------------------------------------------------------------------------
void beginWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(500);

  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  if (strlen(WIFI_PASSWORD) == 0) {
    WiFi.begin(WIFI_SSID);              // For open networks after registration, if applicable.
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
  } else {
    Serial.print("WiFi connection failed. Status code: ");
    Serial.println(WiFi.status());
  }
}

void maintainWiFi()
{
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (now - lastWifiReconnectMs >= wifiReconnectIntervalMs) {
    lastWifiReconnectMs = now;

    Serial.println("WiFi disconnected. Reconnecting...");

    if (strlen(WIFI_PASSWORD) == 0) {
      WiFi.begin(WIFI_SSID);
    } else {
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// -----------------------------------------------------------------------------
// LED pattern helpers
// -----------------------------------------------------------------------------
void setAllLeds(bool on)
{
  for (int i = 0; i < ledCount; i++) {
    digitalWrite(ledPins[i], on ? HIGH : LOW);
  }
}

void setSingleLed(int index)
{
  for (int i = 0; i < ledCount; i++) {
    digitalWrite(ledPins[i], i == index ? HIGH : LOW);
  }
}

void runSolidPattern()
{
  setAllLeds(true);
}

void runBlinkPattern()
{
  const unsigned long interval = 500;
  static bool ledState = false;

  if (millis() - lastPatternUpdateMs >= interval) {
    lastPatternUpdateMs = millis();
    ledState = !ledState;
    setAllLeds(ledState);
  }
}

void runChasePattern()
{
  const unsigned long interval = 250;
  static int index = 0;

  if (millis() - lastPatternUpdateMs >= interval) {
    lastPatternUpdateMs = millis();
    setSingleLed((ledCount - 1) - index);
    index = (index + 1) % ledCount;
  }
}

void runRainbowPattern()
{
  const unsigned long interval = 350;
  static int index = 0;

  if (millis() - lastPatternUpdateMs >= interval) {
    lastPatternUpdateMs = millis();
    setSingleLed(index);
    index = (index + 1) % ledCount;
  }
}

void runFirePattern()
{
  const unsigned long interval = 120;

  if (millis() - lastPatternUpdateMs >= interval) {
    lastPatternUpdateMs = millis();

    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, random(0, 100) > 25 ? HIGH : LOW);
    digitalWrite(greenLedPin, random(0, 100) > 75 ? HIGH : LOW);
  }
}

void runTemperaturePattern()
{
  // Temperature mode uses a simple threshold display:
  // green = cool, yellow = warm, red = hot.
  if (!latestReadingValid || isnan(latestTemperatureC)) {
    setAllLeds(false);
    return;
  }

  if (latestTemperatureC >= 30.0) {
    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
  } else if (latestTemperatureC >= 24.0) {
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
  } else {
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
  }
}

void updateLedPattern()
{
  if (currentPattern == "solid") {
    runSolidPattern();
  } else if (currentPattern == "blink") {
    runBlinkPattern();
  } else if (currentPattern == "chase") {
    runChasePattern();
  } else if (currentPattern == "rainbow") {
    runRainbowPattern();
  } else if (currentPattern == "fire") {
    runFirePattern();
  } else if (currentPattern == "temperature") {
    runTemperaturePattern();
  } else {
    runSolidPattern();
  }
}

// -----------------------------------------------------------------------------
// Thermistor reading
// -----------------------------------------------------------------------------
float readTemperatureC()
{
  latestAdc = analogRead(thermistorPin);
  latestVoltage = latestAdc * referenceVoltage / ADC_MAX;

  // Reject readings that are too close to 0 V or 3.3 V because they normally
  // indicate a disconnected wire, short circuit, or invalid voltage divider.
  if (latestVoltage <= 0.01 || latestVoltage >= referenceVoltage - 0.01) {
    latestResistance = NAN;
    latestReadingValid = false;
    return NAN;
  }

  // Voltage divider equation: calculates the thermistor resistance from the ADC
  // voltage using the known fixed resistor value.
  latestResistance = (latestVoltage * referenceResistor) /
                     (referenceVoltage - latestVoltage);

  if (latestResistance <= 0 || isnan(latestResistance)) {
    latestReadingValid = false;
    return NAN;
  }

  // Beta equation: converts thermistor resistance to temperature in Kelvin,
  // then the result is converted to Celsius.
  float tempK = 1.0 / ((log(latestResistance / nominalResistance) / beta) +
                       (1.0 / (nominalTemperature + 273.15)));

  latestReadingValid = true;
  return tempK - 273.15;
}

// -----------------------------------------------------------------------------
// Flask server communication
// -----------------------------------------------------------------------------
String buildUrl(const String& path)
{
  return String(FLASK_SERVER_BASE_URL) + path;
}

void postSensorData()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping POST: WiFi not connected");
    return;
  }

  HTTPClient http;
  String url = buildUrl("/api/sensor");

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  // Build the JSON payload using ArduinoJson instead of manually joining strings.
  // This is safer because ArduinoJson handles valid JSON formatting automatically.
  JsonDocument doc;

  if (latestReadingValid && !isnan(latestTemperatureC)) {
    doc["temperature_c"] = latestTemperatureC;
  } else {
    doc["temperature_c"] = nullptr;
  }

  doc["adc"] = latestAdc;
  doc["voltage"] = latestVoltage;

  if (latestReadingValid && !isnan(latestResistance)) {
    doc["resistance"] = latestResistance;
  } else {
    doc["resistance"] = nullptr;
  }

  doc["valid"] = latestReadingValid;

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);

  Serial.print("POST /api/sensor -> ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("POST response: ");
    Serial.println(response);
  } else {
    Serial.print("POST failed: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

void getPatternFromServer()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping GET: WiFi not connected");
    return;
  }

  HTTPClient http;
  String url = buildUrl("/api/pattern");

  http.begin(url);
  http.setTimeout(3000);

  int httpCode = http.GET();

  Serial.print("GET /api/pattern -> ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("Failed to parse pattern JSON: ");
      Serial.println(error.c_str());
    } else {
      const char* patternValue = doc["pattern"];

      if (patternValue != nullptr) {
        String pattern = String(patternValue);
        pattern.trim();
        pattern.toLowerCase();

        if (pattern.length() > 0) {
          currentPattern = pattern;
          Serial.print("Current pattern updated to: ");
          Serial.println(currentPattern);
        }
      }
    }
  } else if (httpCode > 0) {
    Serial.print("GET response: ");
    Serial.println(http.getString());
  } else {
    Serial.print("GET failed: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// -----------------------------------------------------------------------------
// Arduino setup and loop
// -----------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== COM3505 IoT Assignment ===");

  WiFi.mode(WIFI_STA);
  Serial.print("ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress());

  analogReadResolution(12);
  analogSetPinAttenuation(thermistorPin, ADC_11db);

  pinMode(redLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  setAllLeds(false);

  randomSeed(analogRead(A0));

  beginWiFi();
}

void loop()
{
  maintainWiFi();

  latestTemperatureC = readTemperatureC();

  Serial.print("ADC: ");
  Serial.print(latestAdc);
  Serial.print("  Voltage: ");
  Serial.print(latestVoltage, 3);
  Serial.print(" V  Resistance: ");

  if (latestReadingValid && !isnan(latestResistance)) {
    Serial.print(latestResistance, 2);
    Serial.print(" ohm");
  } else {
    Serial.print("nan");
  }

  Serial.print("  Temp: ");

  if (latestReadingValid && !isnan(latestTemperatureC)) {
    Serial.print(latestTemperatureC, 2);
    Serial.print(" C");
  } else {
    Serial.print("nan C");
  }

  Serial.print("  Pattern: ");
  Serial.println(currentPattern);

  updateLedPattern();

  unsigned long now = millis();

  if (now - lastSensorPostMs >= sensorPostIntervalMs) {
    lastSensorPostMs = now;
    postSensorData();
  }

  if (now - lastPatternGetMs >= patternGetIntervalMs) {
    lastPatternGetMs = now;
    getPatternFromServer();
  }

  delay(50);
}
