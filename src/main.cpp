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

// Shift register pins.
const int shiftRegisterLatch = D6; // ST_CP
const int shiftRegisterClock = D7; // SH_CP
const int shiftRegisterData = D8;  // DS

const int numShiftLEDs = 16; // two 8-bit registers

// Variable holding current output state for the shift register.
uint16_t shiftRegisterValue = 0;

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

// Helper: Update the shift register outputs.
// Sends 16 bits (high byte first, then low byte).
void updateShiftRegister()
{
  digitalWrite(shiftRegisterLatch, LOW);
  shiftOut(shiftRegisterData, shiftRegisterClock, MSBFIRST, highByte(shiftRegisterValue));
  shiftOut(shiftRegisterData, shiftRegisterClock, MSBFIRST, lowByte(shiftRegisterValue));
  digitalWrite(shiftRegisterLatch, HIGH);
}

// Update external LEDs: for each of the 16 outputs, randomly toggle a bit.
void updateExternalLEDs()
{
  if (blinkingEnabled)
  {
    for (int i = 0; i < numShiftLEDs; i++)
    {
      if (random(100) < blinkingChance)
      {
        // Toggle bit i.
        bool bitVal = bitRead(shiftRegisterValue, i);
        bitWrite(shiftRegisterValue, i, !bitVal);
      }
    }
    updateShiftRegister();
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

  // Set shift register pins as outputs.
  pinMode(shiftRegisterLatch, OUTPUT);
  pinMode(shiftRegisterClock, OUTPUT);
  pinMode(shiftRegisterData, OUTPUT);
  // Initialize shift register (all outputs off).
  shiftRegisterValue = 0;
  updateShiftRegister();

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
      externalLedTicker.detach();
      externalLedTicker.attach_ms(interval, updateExternalLEDs);
      request->send(200, "text/plain", "Blinking started"); });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request)
            {
        blinkingEnabled = false;
        externalLedTicker.detach();
        // Turn off all shift register outputs.
        shiftRegisterValue = 0;
        updateShiftRegister();
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