#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>    // New async server include
#include <ESPAsyncTCP.h>          // Required library for async server
#include <Ticker.h>               // Required library for pulsating LED
#include <ShiftRegister74HC595.h> // Shift register library

// Global ticker for external LED blinking.
Ticker externalLedTicker;
// Global ticker for onboard LED update.
Ticker onboardLedTicker;
// Global ticker for fire PWM effect.
Ticker fireTicker;
// Global ticker for demo mode.
Ticker demoTicker;

// Shift register pins.
const int SHIFT_REGISTER_DATA_PIN = D5;  // DS
const int SHIFT_REGISTER_LATCH_PIN = D6; // ST_CP
const int SHIFT_REGISTER_CLOCK_PIN = D7; // SH_CP

const int NUM_SHIFT_LEDS = 16; // Two cascaded 8-bit registers

ShiftRegister74HC595<2> shiftRegs(SHIFT_REGISTER_DATA_PIN, SHIFT_REGISTER_CLOCK_PIN, SHIFT_REGISTER_LATCH_PIN);

// Pin for the pulsating LED.
const int PULSING_LED_PIN = D3;
const int PULSING_MAX_PWM = 1023;
// Fading LED value
unsigned int pulsePWM = 0; // PWM step (0-1023)

// New pin for "fire" PWM effect.
const int FIRE_LED_PIN = D2;
const int FIRE_REFRESH_INTERVAL = 20;    // Fade interval (ms)
const unsigned long FIRE_DURATION = 700; // Fade duration (ms)
unsigned long fireStartTime = 0;

// Timing variables for external LEDs.
unsigned long blinkingInterval = 200;
long int blinkingChance = 30; // in percent
bool blinkingEnabled = false;

// Create an async web server on port 80.
AsyncWebServer server(80);
AsyncCorsMiddleware *cors = new AsyncCorsMiddleware();

// Update external LEDs: for each of the 16 outputs, randomly toggle a bit.
void updateExternalLEDs()
{
  if (blinkingEnabled)
  {
    for (int i = 0; i < NUM_SHIFT_LEDS; i++)
    {
      if (random(100) < blinkingChance)
      {
        shiftRegs.set(i, !shiftRegs.get(i));
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
  unsigned long elapsed = millis() - fireStartTime;
  if (elapsed < FIRE_DURATION)
  {
    int pwmValue = map(FIRE_DURATION - elapsed, 0, FIRE_DURATION, 0, PULSING_MAX_PWM);
    analogWrite(FIRE_LED_PIN, pwmValue);
  }
  else
  {
    analogWrite(FIRE_LED_PIN, 0); // Turn off fire LED
    fireTicker.detach();          // Stop fire sequence updates
  }
}

void initLEDCheck()
{
  Serial.println("Starting LED check sequence...");

  // Test pulsing LED.
  Serial.println("Testing pulsating LED");
  analogWrite(PULSING_LED_PIN, 0);
  delay(100);
  analogWrite(PULSING_LED_PIN, PULSING_MAX_PWM / 4);
  delay(100);
  analogWrite(PULSING_LED_PIN, (PULSING_MAX_PWM / 4) * 2);
  delay(100);
  analogWrite(PULSING_LED_PIN, (PULSING_MAX_PWM / 4) * 3);
  delay(100);
  analogWrite(PULSING_LED_PIN, PULSING_MAX_PWM);
  delay(500);

  // Test fire LED.
  Serial.println("Testing fire LED");
  analogWrite(FIRE_LED_PIN, 0);
  delay(100);
  analogWrite(FIRE_LED_PIN, PULSING_MAX_PWM / 4);
  delay(100);
  analogWrite(FIRE_LED_PIN, (PULSING_MAX_PWM / 4) * 2);
  delay(100);
  analogWrite(FIRE_LED_PIN, (PULSING_MAX_PWM / 4) * 3);
  delay(100);
  analogWrite(FIRE_LED_PIN, PULSING_MAX_PWM);
  delay(500);

  shiftRegs.setAllLow();
  // Test all shift register LEDs.
  Serial.println("Testing shift register LEDs");
  for (int i = 0; i < NUM_SHIFT_LEDS; i++)
  {
    shiftRegs.set(i, HIGH);
    delay(100);
  }
  delay(500);
  shiftRegs.setAllLow();
  analogWrite(PULSING_LED_PIN, 0);
  analogWrite(FIRE_LED_PIN, 0);
  delay(300);
  shiftRegs.setAllHigh();
  analogWrite(PULSING_LED_PIN, PULSING_MAX_PWM);
  analogWrite(FIRE_LED_PIN, PULSING_MAX_PWM);
  delay(300);
  shiftRegs.setAllLow();
  analogWrite(PULSING_LED_PIN, 0);
  analogWrite(FIRE_LED_PIN, 0);
  delay(300);
  shiftRegs.setAllHigh();
  analogWrite(PULSING_LED_PIN, PULSING_MAX_PWM);
  analogWrite(FIRE_LED_PIN, PULSING_MAX_PWM);

  Serial.println("LED check sequence completed");
}

void startBlinking()
{
  blinkingEnabled = true;
  // Start the ticker callback with the current interval.
  externalLedTicker.detach();
  externalLedTicker.attach_ms(blinkingInterval, updateExternalLEDs);
}

void stopBlinking()
{
  blinkingEnabled = false;
  externalLedTicker.detach();
  shiftRegs.setAllLow();
}

void demoTickerCallback()
{
  // Randomly update the pulsing LED brightness.
  int randomPWM = random(0, PULSING_MAX_PWM + 1);
  analogWrite(PULSING_LED_PIN, randomPWM);

  // With a 10% chance, trigger the fire LED effect.
  if (random(100) < 10)
  {
    fireStartTime = millis();
    analogWrite(FIRE_LED_PIN, PULSING_MAX_PWM);
    fireTicker.detach();
    fireTicker.attach_ms(FIRE_REFRESH_INTERVAL, updateFireSequence);
  }
}

void startDemoMode()
{
  Serial.println("Demo mode enabled");
  startBlinking();
  demoTicker.attach(1.0, demoTickerCallback);
}

void stopDemoMode()
{
  Serial.println("Demo mode disabled");
  stopBlinking();
  demoTicker.detach();
  analogWrite(PULSING_LED_PIN, 0);
  analogWrite(FIRE_LED_PIN, 0);
}

void setup()
{
  Serial.begin(115200);

  initLEDCheck();

  // Set shift register pins as outputs.
  pinMode(SHIFT_REGISTER_LATCH_PIN, OUTPUT);
  pinMode(SHIFT_REGISTER_CLOCK_PIN, OUTPUT);
  pinMode(SHIFT_REGISTER_DATA_PIN, OUTPUT);

  // Initialize onboard LED.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Initialize pulsating LED and fire PWM pin.
  analogWriteRange(PULSING_MAX_PWM);
  pinMode(PULSING_LED_PIN, OUTPUT);
  pinMode(FIRE_LED_PIN, OUTPUT);
  analogWrite(FIRE_LED_PIN, 0);
  analogWrite(PULSING_LED_PIN, 0);

  shiftRegs.setAllLow();

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
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String json = "{\"blinkingEnabled\": " + String(blinkingEnabled ? "true" : "false") + ",";
    json += "\"fadeAmount\": " + String(pulsePWM) + ",";
    json += "\"blinkingInterval\": " + String(blinkingInterval) + ",";
    json += "\"blinkingChance\": " + String(blinkingChance) + "}";
    request->send(200, "application/json", json); });

  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    startBlinking();
    request->send(200, "text/plain", "Blinking started"); });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    stopBlinking();
    request->send(200, "text/plain", "Blinking stopped"); });

  server.on("/demoon", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    startDemoMode();
    request->send(200, "text/plain", "Demo started"); });

  server.on("/demooff", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    stopDemoMode();
    request->send(200, "text/plain", "Demo started"); });

  server.on("/setfade", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasArg("fade")) {
      pulsePWM = request->arg("fade").toInt();
      analogWrite(PULSING_LED_PIN, pulsePWM);
      request->send(200, "text/plain", "Fade amount updated: " + String(pulsePWM));
    } else {
      request->send(400, "text/plain", "Missing fade parameter");
    } });

  server.on("/setinterval", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasArg("interval")) {
      blinkingInterval = request->arg("interval").toInt();
      // If blinking is active, update the ticker period.
      if (blinkingEnabled) {
        externalLedTicker.detach();
        externalLedTicker.attach_ms(blinkingInterval, updateExternalLEDs);
      }
      request->send(200, "text/plain", "Blink interval updated: " + String(blinkingInterval) + " ms");
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
    fireStartTime = millis();
    analogWrite(FIRE_LED_PIN, PULSING_MAX_PWM);
    // Attach the ticker to update fire sequence every 50ms.
    fireTicker.detach();
    fireTicker.attach_ms(FIRE_REFRESH_INTERVAL, updateFireSequence);
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
  startDemoMode();
}

void loop()
{
}