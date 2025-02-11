#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h> // New async server include
#include <ESPAsyncTCP.h>       // Required library for async server
#include <Ticker.h>            // Required library for pulsating LED

// Global ticker for external LED blinking.
Ticker externalLedTicker;
// Global ticker for onboard LED update.
Ticker onboardLedTicker;
// Global ticker for fire PWM effect.
Ticker fireTicker;

// Specify the array of D-pins connected to your external LEDs.
const int ledPins[] = {D8, D7, D6, D5, D3}; // <-- Change or add pins as needed.
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);

// Pin for the pulsating LED.
const int pulsatingPin = D1;
const int pulsatingMaxPWM = 1023;

// New pin for "fire" PWM effect.
const int firePin = D2;

// Timing variables for external LEDs.
unsigned long interval = 200;
unsigned long previousMillis = 0;
int blinkingChance = 30; // in percent

// Other globals.
int fadeAmount = 5; // PWM step (0-1023)
bool blinkingEnabled = false;
bool firingSequence = false;
unsigned long fireStartTime = 0;
const unsigned long fireDuration = 1000; // Fade duration (ms)

// Create an async web server on port 80.
AsyncWebServer server(80);
AsyncCorsMiddleware *cors = new AsyncCorsMiddleware();

void updateExternalLEDs()
{
  if (blinkingEnabled)
  {
    // For each LED, randomly toggle based on blinkingChance.
    for (int i = 0; i < numLeds; i++)
    {
      if (random(100) < blinkingChance)
      {
        digitalWrite(ledPins[i], !digitalRead(ledPins[i]));
      }
    }
  }
}

void updateOnboardLED()
{
  // If any device is connected, keep LED steady on.
  if (WiFi.softAPgetStationNum() > 0)
  {
    digitalWrite(LED_BUILTIN, LOW); // LOW turns LED on.
  }
  else
  {
    // Else, toggle the onboard LED.
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void updateFireSequence()
{
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
      analogWrite(firePin, 0); // Turn off fire LED
      firingSequence = false;
      fireTicker.detach(); // Stop fire sequence updates
    }
  }
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
  digitalWrite(LED_BUILTIN, HIGH);

  // Initialize pulsating LED and fire PWM pin.
  pinMode(pulsatingPin, OUTPUT);
  analogWriteRange(1023);
  pinMode(firePin, OUTPUT);
  digitalWrite(firePin, LOW);

  // Seed random number generator.
  randomSeed(analogRead(A0));

  // Start Wi-Fi access point.
  const char *ssid = "Exocomp";
  const char *password = "exocomp42";
  WiFi.softAP(ssid, password);
  Serial.print("\nAP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.addMiddleware(cors);

  // Setup async web server routes.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String html = "<html><head><title>LED Status</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "</head><body>";
    html += "<h1>LED Status</h1>";
    html += "<p>External LED Blinking: " + String(blinkingEnabled ? "Enabled" : "Disabled") + "</p>";
    html += "<p>Pulsating LED Fade Amount: " + String(fadeAmount) + "</p>";
    html += "<p>Blink Interval: " + String(interval) + " ms</p>";
    html += "<p>Blinking Chance: " + String(blinkingChance) + "%</p>";
    html += "</body></html>";
    request->send(200, "text/html", html); });

  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      blinkingEnabled = true;
      // Start the ticker callback with the current interval.
      externalLedTicker.attach_ms(interval, updateExternalLEDs);
      request->send(200, "text/plain", "Blinking started"); });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      blinkingEnabled = false;
      externalLedTicker.detach(); // Stop the ticker callback.
      for (int i = 0; i < numLeds; i++) {
        digitalWrite(ledPins[i], LOW);
      }
      request->send(200, "text/plain", "Blinking stopped"); });

  server.on("/setfade", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasArg("fade")) {
      fadeAmount = request->arg("fade").toInt();
      analogWrite(pulsatingPin, fadeAmount);
      request->send(200, "text/plain", "Fade amount updated: " + String(fadeAmount));
    } else {
      request->send(400, "text/plain", "Missing fade parameter");
    } });

  server.on("/setinterval", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      if (request->hasArg("interval")) {
        interval = request->arg("interval").toInt();
        // If blinking is active, update the ticker period.
        if (blinkingEnabled) {
          externalLedTicker.detach();
          externalLedTicker.attach_ms(interval, updateExternalLEDs);
        }
        request->send(200, "text/plain", "Blink interval updated: " + String(interval) + " ms");
      } else {
        request->send(400, "text/plain", "Missing interval parameter");
      } });

  server.on("/setblinkchance", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasArg("chance")) {
      blinkingChance = request->arg("chance").toInt();
      request->send(200, "text/plain", "Blinking chance updated: " + String(blinkingChance) + "%");
    } else {
      request->send(400, "text/plain", "Missing chance parameter");
    } });

  server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      firingSequence = true;
      fireStartTime = millis();
      analogWrite(firePin, pulsatingMaxPWM);
      // Attach the ticker to update fire sequence every 50ms.
      fireTicker.detach();
      fireTicker.attach_ms(50, updateFireSequence);
      request->send(200, "text/plain", "Fire sequence initiated"); });

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    request->send(200, "text/plain", "Restarting device...");
    delay(100); // Allow time for the response to be sent
    ESP.restart(); });

  server.begin();
  Serial.println("Async HTTP server started");

  // Start the onboard LED ticker to update every second.
  onboardLedTicker.attach(0.5, updateOnboardLED);
}

void loop()
{
}