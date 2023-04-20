#include <Arduino.h>

int relay = 2; 

void setup() {

pinMode(relay, OUTPUT);
digitalWrite(relay, LOW);

}

void loop() {

digitalWrite(relay, LOW);
delay(1000);
digitalWrite(relay, HIGH);
delay(1000);

}
