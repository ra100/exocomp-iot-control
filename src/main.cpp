#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("D1 mini is alive!");
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("LED ON");
  delay(500);

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("LED OFF");
  delay(500);
}
