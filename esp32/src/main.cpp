// COM3505 IoT Assignment
// ESP32-S3 Feather + NTC Thermistor + 3 LED Patterns + Flask Server

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

#include "secrets.h"

// ---------------- Pin Configuration ----------------
// Adafruit ESP32-S3 Feather:
// A1 = GPIO17, A2 = GPIO16, A3 = GPIO15, D5 = GPIO5, D6 = GPIO6, D9 = GPIO9

const int thermistorPin = A2;   // Same as your working Ex12.cpp

const int redLedPin = 5;        // D5
const int yellowLedPin = 6;     // D6
const int greenLedPin = 9;      // D9

const int ledPins[] = { redLedPin, yellowLedPin, greenLedPin };
const int ledCount = sizeof(ledPins) / sizeof(ledPins[0]);

// ---------------- Thermistor Configuration ----------------
// Same values as your working Ex12.cpp
const float referenceVoltage = 3.3;
const float referenceResistor = 180.0;
const float beta = 3950.0;
const float nominalTemperature = 25.0;
const float nominalResistance = 180.0;

#define ADC_MAX 4095.0

// ---------------- Timing ----------------

const unsigned long sensorPostIntervalMs = 2000;
const unsigned long patternGetIntervalMs = 2000;
const unsigned long wifiReconnectIntervalMs = 5000;

unsigned long lastSensorPostMs = 0;
unsigned long lastPatternGetMs = 0;
unsigned long lastWifiReconnectMs = 0;
unsigned long lastPatternUpdateMs = 0;

// ---------------- State ----------------

String currentPattern = "solid";

float latestTemperatureC = NAN;
int latestAdc = 0;
float latestVoltage = 0.0;
float latestResistance = NAN;
bool latestReadingValid = false;

// ---------------- WiFi ----------------

void beginWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(500);

  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  if (strlen(WIFI_PASSWORD) == 0) {
    WiFi.begin(WIFI_SSID);              // ASK4 Wireless after MAC registration
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

// ---------------- LEDs ----------------

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
    setSingleLed(index);
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
  if (!latestReadingValid || isnan(latestTemperatureC)) {
    setAllLeds(false);
    return;
  }

  if (latestTemperatureC >= 30.0) {
    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
  } else if (latestTemperatureC >= 25.0) {
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

// ---------------- Thermistor ----------------

float readTemperatureC()
{
  // Same thermistor calculation as your working Ex12.cpp
  latestAdc = analogRead(thermistorPin);

  latestVoltage = latestAdc * referenceVoltage / ADC_MAX;

  if (latestVoltage <= 0.01 || latestVoltage >= referenceVoltage - 0.01) {
    latestResistance = NAN;
    latestReadingValid = false;
    return NAN;
  }

  // Voltage divider equation from Ex12.cpp
  latestResistance = (latestVoltage * referenceResistor) /
                     (referenceVoltage - latestVoltage);

  if (latestResistance <= 0 || isnan(latestResistance)) {
    latestReadingValid = false;
    return NAN;
  }

  // Beta equation from Ex12.cpp
  float tempK = 1.0 / ((log(latestResistance / nominalResistance) / beta) +
                       (1.0 / (nominalTemperature + 273.15)));

  latestReadingValid = true;
  return tempK - 273.15;
}

// ---------------- Flask Communication ----------------

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

  String payload = "{";
  payload += "\"temperature_c\":";

  if (latestReadingValid && !isnan(latestTemperatureC)) {
    payload += String(latestTemperatureC, 2);
  } else {
    payload += "null";
  }

  payload += ",\"adc\":";
  payload += String(latestAdc);

  payload += ",\"voltage\":";
  payload += String(latestVoltage, 3);

  payload += ",\"resistance\":";

  if (latestReadingValid && !isnan(latestResistance)) {
    payload += String(latestResistance, 2);
  } else {
    payload += "null";
  }

  payload += ",\"valid\":";
  payload += latestReadingValid ? "true" : "false";

  payload += "}";

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
    String pattern = http.getString();
    pattern.trim();
    pattern.toLowerCase();

    if (pattern.length() > 0) {
      currentPattern = pattern;
      Serial.print("Current pattern updated to: ");
      Serial.println(currentPattern);
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

// ---------------- Setup and Loop ----------------

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