#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Specify the array of D-pins connected to your external LEDs.
const int ledPins[] = {D8, D7, D6};  // <-- Change or add pins as needed.
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);

// Pin for the pulsating LED.
const int pulsatingPin = D5;
const int pulsatingMaxPWM = 512;

// Timing variables for external LEDs.
unsigned long previousMillis = 0;
const unsigned long interval = 200;

// Timing variables for onboard LED.
unsigned long previousMillisOnboard = 0;
const unsigned long onboardInterval = 500; // Blink onboard LED every 500ms

// Timing variables for pulsating LED.
unsigned long previousMillisPulse = 0;
const unsigned long pulseInterval = 30; // Adjust this value for fade speed.

int brightness = 0;
int fadeAmount = 5; // PWM step (0-1023 range for ESP8266)

// Global flag to control blinking of external LEDs.
bool blinkingEnabled = false;

// Create a web server on port 80.
ESP8266WebServer server(80);

void handleRoot() {
  String html = "<html><head><title>LED Blinking Control</title></head><body>";
  html += "<h1>LED Blinking Control</h1>";
  html += "<p>Click a button to start or stop external LED blinking.</p>";
  html += "<form action='/start' method='get'><input type='submit' value='Start Blinking'></form>";
  html += "<form action='/stop' method='get'><input type='submit' value='Stop Blinking'></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStart() {
  blinkingEnabled = true;
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Blinking started");
}

void handleStop() {
  blinkingEnabled = false;
  // Turn off all external LEDs when stopping.
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "Blinking stopped");
}

void setup() {
  Serial.begin(115200);

  // Initialize external LED pins.
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Initialize onboard LED.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Note: On many ESP8266 boards, LOW turns the LED on.

  // Initialize pulsating LED pin.
  pinMode(pulsatingPin, OUTPUT);
  analogWriteRange(1023); // Ensure the PWM range is set to 0-1023.

  // Seed the random number generator.
  randomSeed(analogRead(A0));

  // Start Wi-Fi access point.
  const char* ssid = "Exocomp";
  const char* password = "exocomp42";
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Setup web server routes.
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle incoming client requests.
  server.handleClient();

  // Blink the onboard LED as long as Wi-Fi is active.
  unsigned long currentMillisOnboard = millis();
  if (currentMillisOnboard - previousMillisOnboard >= onboardInterval) {
    previousMillisOnboard = currentMillisOnboard;
    // Toggle onboard LED. (For many ESP8266 boards, LOW is on, HIGH is off.)
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // Only perform external LED blinking if enabled.
  if (blinkingEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      // For each external LED, decide whether to toggle its state (30% chance).
      for (int i = 0; i < numLeds; i++) {
        if (random(100) < 30) {
          digitalWrite(ledPins[i], !digitalRead(ledPins[i]));
        }
      }
    }
  }

  // Pulsate the single LED on pulsatingPin.
  unsigned long currentMillisPulse = millis();
  if (currentMillisPulse - previousMillisPulse >= pulseInterval) {
    previousMillisPulse = currentMillisPulse;
    brightness += fadeAmount;
    // Reverse the direction of the fading at the ends.
    if (brightness <= 0 || brightness >= pulsatingMaxPWM) {
      fadeAmount = -fadeAmount;
    }
    analogWrite(pulsatingPin, brightness);
  }
}