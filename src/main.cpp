#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Specify the array of D-pins connected to your external LEDs.
const int ledPins[] = {D8, D7, D6, D5, D3}; // <-- Change or add pins as needed.
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);

// Pin for the pulsating LED.
const int pulsatingPin = D1;
const int pulsatingMaxPWM = 1023;

// New pin for "fire" PWM effect.
const int firePin = D2;

// Timing variables for external LEDs.
// Made mutable so they can be updated at runtime.
unsigned long interval = 200;
unsigned long previousMillis = 0;
int blinkingChance = 30; // Blinking chance in percent (0-100)

// Timing variables for onboard LED.
unsigned long previousMillisOnboard = 0;
const unsigned long onboardInterval = 500; // Blink onboard LED every 500ms

int brightness = 0;
int fadeAmount = 5; // PWM step (0-1023 range for ESP8266)

// Global flag to control blinking of external LEDs.
bool blinkingEnabled = false;

bool firingSequence = false;
unsigned long fireStartTime = 0;
const unsigned long fireDuration = 1000; // Fade duration (ms)

// Create a web server on port 80.
ESP8266WebServer server(80);

void handleRoot()
{
  // Build a status page showing the current state.
  String html = "<html><head><title>LED Status</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "</head><body>";
  html += "<h1>LED Status</h1>";
  html += "<p>External LED Blinking: ";
  html += (blinkingEnabled ? "Enabled" : "Disabled");
  html += "</p>";
  html += "<p>Pulsating LED Fade Amount: " + String(fadeAmount) + "</p>";
  html += "<p>Blink Interval: " + String(interval) + " ms</p>";
  html += "<p>Blinking Chance: " + String(blinkingChance) + "%</p>";
  html += "</body></html>";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", html);
}

void handleStart()
{
  blinkingEnabled = true;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Blinking started");
}

void handleStop()
{
  blinkingEnabled = false;
  // Turn off all external LEDs when stopping.
  for (int i = 0; i < numLeds; i++)
  {
    digitalWrite(ledPins[i], LOW);
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Blinking stopped");
}

void handleSetFade()
{
  if (server.hasArg("fade"))
  {
    fadeAmount = server.arg("fade").toInt();
    analogWrite(pulsatingPin, fadeAmount);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Fade amount updated: " + String(fadeAmount));
  }
  else
  {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(400, "text/plain", "Missing fade parameter");
  }
}

void handleSetInterval()
{
  if (server.hasArg("interval"))
  {
    interval = server.arg("interval").toInt();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Blink interval updated: " + String(interval) + " ms");
  }
  else
  {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(400, "text/plain", "Missing interval parameter");
  }
}

void handleSetBlinkChance()
{
  if (server.hasArg("chance"))
  {
    blinkingChance = server.arg("chance").toInt();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Blinking chance updated: " + String(blinkingChance) + "%");
  }
  else
  {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(400, "text/plain", "Missing chance parameter");
  }
}

void handleFire()
{
  firingSequence = true;
  fireStartTime = millis();
  // Start at max PWM.
  analogWrite(firePin, pulsatingMaxPWM);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Fire sequence initiated");
}

void handleRestart()
{
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Restarting device...");
  delay(100); // Allow time for the response to be sent
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);

  // Initialize external LED pins.
  for (int i = 0; i < numLeds; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Initialize onboard LED.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Note: On many ESP8266 boards, LOW turns the LED on.

  // Initialize pulsating LED pin.
  pinMode(pulsatingPin, OUTPUT);
  analogWriteRange(1023); // Ensure the PWM range is set to 0-1023.

  // Initialize fire PWM pin.
  pinMode(firePin, OUTPUT);
  digitalWrite(firePin, LOW);

  // Seed the random number generator.
  randomSeed(analogRead(A0));

  // Start Wi-Fi access point.
  const char *ssid = "Exocomp";
  const char *password = "exocomp42";
  WiFi.softAP(ssid, password);
  Serial.print("\nAP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Setup web server routes.
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/setfade", handleSetFade);
  server.on("/setinterval", handleSetInterval);
  server.on("/setblinkchance", handleSetBlinkChance);
  server.on("/fire", handleFire);
  server.on("/restart", handleRestart);
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  // Handle incoming client requests.
  server.handleClient();

  // Process the non-blocking fire sequence.
  if (firingSequence)
  {
    unsigned long elapsed = millis() - fireStartTime;
    if (elapsed < fireDuration)
    {
      int pwmValue = map(fireDuration - elapsed, 0, fireDuration, 0, pulsatingMaxPWM);
      analogWrite(firePin, pwmValue);
    }
    else
    {
      analogWrite(firePin, 0); // Ensure it's off
      firingSequence = false;
    }
  }

  // Blink the onboard LED as long as Wi-Fi is active.
  unsigned long currentMillisOnboard = millis();
  if (currentMillisOnboard - previousMillisOnboard >= onboardInterval)
  {
    previousMillisOnboard = currentMillisOnboard;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // Only perform external LED blinking if enabled.
  if (blinkingEnabled)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis = currentMillis;
      for (int i = 0; i < numLeds; i++)
      {
        if (random(100) < blinkingChance)
        {
          digitalWrite(ledPins[i], !digitalRead(ledPins[i]));
        }
      }
    }
  }
}