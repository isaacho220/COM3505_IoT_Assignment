// COM3505 IoT Assignment
// ESP32-S3 Feather + NTC Thermistor + 3 LED Patterns + Flask Server

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "secrets.h"

// ---------------- Pin Configuration ----------------
// Adafruit ESP32-S3 Feather pinout:
// A1 = GPIO17, A2 = GPIO16, D5 = GPIO5, D6 = GPIO6, D9 = GPIO9.
// If A1/D5 names do not compile on your machine, replace them with 17/5/etc.
const int thermistorPin = A1;
const int redLedPin = 5;      // D5
const int yellowLedPin = 6;   // D6
const int greenLedPin = 9;    // D9

const int ledPins[] = { redLedPin, yellowLedPin, greenLedPin };
const int ledCount = sizeof(ledPins) / sizeof(ledPins[0]);

// ---------------- Thermistor Configuration ----------------
// Change referenceResistor to match the fixed resistor used in your voltage divider.
// If your working circuit uses 180 ohms, keep 180.0.
// If you later use 4.7k, change it to 4700.0.
// If you later use 10k, change it to 10000.0.
const float referenceVoltage = 3.3;
const float referenceResistor = 180.0;       // fixed resistor in the voltage divider
const float beta = 3950.0;
const float nominalTemperature = 25.0;
const float nominalResistance = 10000.0;     // typical 10k NTC at 25 C
#define ADC_MAX 4095.0

// Set this depending on how your NTC divider is wired.
// true  = 3.3V -> NTC -> ADC pin -> fixed resistor -> GND
// false = 3.3V -> fixed resistor -> ADC pin -> NTC -> GND
const bool NTC_ON_TOP = true;

// ---------------- Timing ----------------
const unsigned long sensorPostIntervalMs = 2000;
const unsigned long patternPollIntervalMs = 2000;
const unsigned long wifiRetryIntervalMs = 5000;

unsigned long lastSensorPostMs = 0;
unsigned long lastPatternPollMs = 0;
unsigned long lastWifiRetryMs = 0;
unsigned long lastPatternStepMs = 0;

// ---------------- Runtime State ----------------
String currentPattern = "solid";
float latestTemperatureC = NAN;
int latestAdcValue = 0;
float latestVoltage = 0.0;
float latestResistance = 0.0;

int chaseIndex = 0;
bool blinkOn = false;
int colourCycleIndex = 0;

// ---------------- Utility Functions ----------------
void setAllLeds(bool on) {
  for (int i = 0; i < ledCount; i++) {
    digitalWrite(ledPins[i], on ? HIGH : LOW);
  }
}

void setOnlyLed(int index) {
  for (int i = 0; i < ledCount; i++) {
    digitalWrite(ledPins[i], i == index ? HIGH : LOW);
  }
}

float readTemperatureC() {
  latestAdcValue = analogRead(thermistorPin);
  latestVoltage = latestAdcValue * referenceVoltage / ADC_MAX;

  if (latestVoltage <= 0.01 || latestVoltage >= referenceVoltage - 0.01) {
    latestResistance = NAN;
    return NAN;
  }

  if (NTC_ON_TOP) {
    // Wiring: 3.3V -> NTC -> ADC -> fixed resistor -> GND
    latestResistance = (latestVoltage * referenceResistor) / (referenceVoltage - latestVoltage);
  } else {
    // Wiring: 3.3V -> fixed resistor -> ADC -> NTC -> GND
    latestResistance = referenceResistor * (referenceVoltage - latestVoltage) / latestVoltage;
  }

  if (latestResistance <= 0 || isnan(latestResistance) || isinf(latestResistance)) {
    return NAN;
  }

  float tempK = 1.0 / ((log(latestResistance / nominalResistance) / beta) +
                       (1.0 / (nominalTemperature + 273.15)));

  return tempK - 273.15;
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed. Will retry in loop.");
  }
}

void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (now - lastWifiRetryMs >= wifiRetryIntervalMs) {
    lastWifiRetryMs = now;
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// ---------------- Flask Communication ----------------
void postSensorDataToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping POST: WiFi not connected");
    return;
  }

  HTTPClient http;
  String url = String(FLASK_SERVER_BASE_URL) + "/api/sensor";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["temperature_c"] = isnan(latestTemperatureC) ? 0.0 : latestTemperatureC;
  doc["adc"] = latestAdcValue;
  doc["voltage"] = latestVoltage;
  doc["resistance"] = isnan(latestResistance) ? 0.0 : latestResistance;
  doc["valid"] = !isnan(latestTemperatureC);

  String requestBody;
  serializeJson(doc, requestBody);

  int responseCode = http.POST(requestBody);

  if (responseCode > 0) {
    Serial.print("POST /api/sensor -> ");
    Serial.println(responseCode);
  } else {
    Serial.print("POST failed: ");
    Serial.println(http.errorToString(responseCode));
  }

  http.end();
}

void pollPatternFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping GET: WiFi not connected");
    return;
  }

  HTTPClient http;
  String url = String(FLASK_SERVER_BASE_URL) + "/api/pattern";

  http.begin(url);
  int responseCode = http.GET();

  if (responseCode == 200) {
    String newPattern = http.getString();
    newPattern.trim();

    if (newPattern.length() > 0 && newPattern != currentPattern) {
      currentPattern = newPattern;
      setAllLeds(false);
      chaseIndex = 0;
      blinkOn = false;
      colourCycleIndex = 0;

      Serial.print("Pattern changed to: ");
      Serial.println(currentPattern);
    }
  } else {
    Serial.print("GET /api/pattern failed: ");
    Serial.println(responseCode);
  }

  http.end();
}

// ---------------- LED Patterns ----------------
void runSolidPattern(unsigned long now) {
  // Simple solid status: all LEDs on.
  setAllLeds(true);
}

void runBlinkPattern(unsigned long now) {
  if (now - lastPatternStepMs >= 500) {
    lastPatternStepMs = now;
    blinkOn = !blinkOn;
    setAllLeds(blinkOn);
  }
}

void runChasePattern(unsigned long now) {
  if (now - lastPatternStepMs >= 300) {
    lastPatternStepMs = now;
    setOnlyLed(chaseIndex);
    chaseIndex = (chaseIndex + 1) % ledCount;
  }
}

void runRainbowPattern(unsigned long now) {
  // With separate red/yellow/green LEDs, rainbow means a repeating colour cycle.
  if (now - lastPatternStepMs >= 450) {
    lastPatternStepMs = now;
    setOnlyLed(colourCycleIndex);
    colourCycleIndex = (colourCycleIndex + 1) % ledCount;
  }
}

void runFirePattern(unsigned long now) {
  // Simple flame/flicker effect using random LED states.
  if (now - lastPatternStepMs >= 120) {
    lastPatternStepMs = now;

    digitalWrite(redLedPin, random(0, 100) < 85 ? HIGH : LOW);
    digitalWrite(yellowLedPin, random(0, 100) < 65 ? HIGH : LOW);
    digitalWrite(greenLedPin, random(0, 100) < 20 ? HIGH : LOW);
  }
}

void runTemperatureAlertPattern(unsigned long now) {
  // Optional simple automated behaviour: high temperature alert.
  // This is available from the web interface as "temperature".
  if (isnan(latestTemperatureC)) {
    setAllLeds(false);
    return;
  }

  if (latestTemperatureC < 25.0) {
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
  } else if (latestTemperatureC < 30.0) {
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
  } else {
    if (now - lastPatternStepMs >= 250) {
      lastPatternStepMs = now;
      blinkOn = !blinkOn;
      digitalWrite(redLedPin, blinkOn ? HIGH : LOW);
      digitalWrite(yellowLedPin, LOW);
      digitalWrite(greenLedPin, LOW);
    }
  }
}

void updateLedPattern() {
  unsigned long now = millis();

  if (currentPattern == "solid") {
    runSolidPattern(now);
  } else if (currentPattern == "blink") {
    runBlinkPattern(now);
  } else if (currentPattern == "chase") {
    runChasePattern(now);
  } else if (currentPattern == "rainbow") {
    runRainbowPattern(now);
  } else if (currentPattern == "fire") {
    runFirePattern(now);
  } else if (currentPattern == "temperature") {
    runTemperatureAlertPattern(now);
  } else {
    runSolidPattern(now);
  }
}

// ---------------- Arduino Lifecycle ----------------
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println("=== COM3505 IoT Assignment: NTC + LEDs + Flask ===");

  analogReadResolution(12);
  analogSetPinAttenuation(thermistorPin, ADC_11db);

  for (int i = 0; i < ledCount; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  randomSeed(analogRead(thermistorPin));
  connectToWiFi();
}

void loop() {
  ensureWiFiConnected();

  unsigned long now = millis();

  // Read sensor frequently enough for local pattern use and serial output.
  latestTemperatureC = readTemperatureC();

  updateLedPattern();

  if (now - lastSensorPostMs >= sensorPostIntervalMs) {
    lastSensorPostMs = now;

    Serial.printf("ADC: %d  Voltage: %.3f V  Resistance: %.2f ohm  Temp: %.2f C  Pattern: %s\n",
                  latestAdcValue,
                  latestVoltage,
                  latestResistance,
                  latestTemperatureC,
                  currentPattern.c_str());

    postSensorDataToServer();
  }

  if (now - lastPatternPollMs >= patternPollIntervalMs) {
    lastPatternPollMs = now;
    pollPatternFromServer();
  }
}
