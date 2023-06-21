#include <Arduino.h>

int solenoid = 2; 

void setup() {
  pinMode(solenoid, OUTPUT);
  digitalWrite(solenoid, LOW);
}

void loop() {
  digitalWrite(solenoid, LOW);
  delay(1000);
  digitalWrite(solenoid, HIGH);
  delay(1000);
}
