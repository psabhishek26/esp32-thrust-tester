#include <WiFi.h>
#include <ESP32Servo.h>
#include "HX711.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

const char* ssid = "espthrust";
const char* password = "12345678";

#define RPM_SENSOR_PIN 4
#define SERVO_PIN 18
#define LOADCELL_PIN 21
#define LOADCELL_DOUT_PIN 22
#define VOLTAGE_PIN 36
#define CURRENT_PIN 39

Servo servo;
HX711 loadCell;
WebServer server(80);

unsigned short throttle = 1000;
int sampleInterval = 1000; // ms
bool sampling = false;
volatile byte rpmcount = 0;
unsigned long lastSampleTime = 0, lastWiFiCheck = 0;

// Debouncing interval for RPM interrupt
unsigned long lastRPMTime = 0;
const unsigned long debounceDelay = 20;

float adc_voltage = 0.0;
float in_voltage = 0.0;
float R1 = 30000.0;
float R2 = 7500.0; 
float ref_voltage = 3.3;
int adc_value = 0;

const float ACS_OFFSET = 2500.0;
const float mVperAmp = 66.0;
int current_adc_value = 0;
float current_sensor_voltage = 0.0;
float current_in_amps = 0.0;

void IRAM_ATTR rpmCounterInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastRPMTime > debounceDelay) { // Debounce RPM signal
    rpmcount++;
    lastRPMTime = currentTime;
  }
}

unsigned int calculateRPM() {
  const int pulsesPerRotation = 2; 
  unsigned long timePassed = millis() - lastSampleTime;
  float minutesPassed = timePassed / 60000.0;
  unsigned int rpm = (rpmcount / pulsesPerRotation) / minutesPassed;
  rpmcount = 0;
  return rpm;
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("Reconnecting to WiFi...");
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Reconnected to WiFi!");
    } else {
      Serial.println("WiFi reconnection failed.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi");
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount file system");
    return;
  }

  servo.attach(SERVO_PIN); 
  servo.writeMicroseconds(1000); 
  
  loadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_PIN);
  loadCell.set_scale();
  loadCell.tare();

  pinMode(RPM_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RPM_SENSOR_PIN), rpmCounterInterrupt, FALLING);

  // Web Server
  server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    servo.writeMicroseconds(1000);
    file.close();
  });

  server.on("/style.css", HTTP_GET, []() {
    File file = SPIFFS.open("/style.css", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/css");
    file.close();
  });

  server.on("/script.js", HTTP_GET, []() {
    File file = SPIFFS.open("/script.js", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "application/javascript");
    file.close();
  });

  server.on("/setThrottle", HTTP_GET, []() {
    if (server.hasArg("value")) {
      throttle = server.arg("value").toInt();
      servo.writeMicroseconds(throttle);
      server.send(200, "text/plain", "Throttle set");
    } else {
      server.send(400, "text/plain", "Missing throttle value");
    }
  });

  server.on("/startSampling", HTTP_GET, []() {
    sampling = true;
    server.send(200, "text/plain", "Sampling started");
  });

  server.on("/stopSampling", HTTP_GET, []() {
    sampling = false;
    server.send(200, "text/plain", "Sampling stopped");
  });

  server.on("/getRawThrust", HTTP_GET, []() {
    StaticJsonDocument<100> doc;
    float rawThrust = loadCell.read();
    doc["thrust"] = rawThrust;

    String jsonData;
    serializeJson(doc, jsonData);
    server.send(200, "application/json", jsonData);
  });

  server.on("/getData", HTTP_GET, []() {
    StaticJsonDocument<200> doc;
    doc["thrust"] = loadCell.read();
    doc["voltage"] = readVoltage();
    doc["current"] = readCurrent();
    doc["rpm"] = calculateRPM();
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    rpmcount = 0; 
    server.send(200, "application/json", jsonData);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  if (millis() - lastWiFiCheck > 5000) { // Check WiFi every 5 seconds
    lastWiFiCheck = millis();
    checkWiFi();
  }

  if (millis() - lastSampleTime > sampleInterval) {
    lastSampleTime = millis();
    
    Serial.print("Throttle: "); Serial.println(throttle);
    Serial.print("Thrust: "); Serial.println(loadCell.read());
    Serial.print("Voltage: "); Serial.println(readVoltage());
    Serial.print("Current: "); Serial.println(readCurrent());
    Serial.print("RPM: "); Serial.println(calculateRPM());
    
    rpmcount = 0; 
  }
}

float readVoltage() {
  adc_value = analogRead(VOLTAGE_PIN);
  adc_voltage  = (adc_value * ref_voltage) / 4095.0; // 12-bit ADC on ESP32
  in_voltage = adc_voltage * (R1 + R2) / R2;
  return in_voltage;
}

float readCurrent() {
  current_adc_value = analogRead(CURRENT_PIN);
  current_sensor_voltage = (current_adc_value * ref_voltage) / 4095.0;
  current_in_amps = (current_sensor_voltage - 2.5) / (mVperAmp / 1000.0);  // Convert mV to V
  return current_in_amps;
}