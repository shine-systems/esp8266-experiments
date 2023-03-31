/*
  ESP8266 Blink by Simon Peter
  Blink the blue LED on the ESP-01 module
  This example code is in the public domain

  The blue LED on the ESP-01 module is connected to GPIO1
  (which is also the TXD pin; so we cannot use Serial.print() at the same time)

  Note that this sketch uses LED_BUILTIN to find the pin with the internal LED
*/

int LED = 5;
int LDR = A0;

float ldr_value = 0.0;

void setup() {
  pinMode(LED,OUTPUT);
  pinMode(LDR,INPUT);  

  Serial.begin(9600); // declare board rate (freq. of comms)

  Serial.println("Scadoopy mcdoopy, babadaboopi...");
}

// the loop function runs over and over again forever
void loop() {
  ldr_value = analogRead(LDR);
  Serial.print("LDR = ");
  Serial.println(ldr_value);

  if (ldr_value < 600) {
    digitalWrite(LED, HIGH);  // Turn the LED off by making the voltage HIGH
  } else {
    digitalWrite(LED, LOW);  // Turn the LED on (Note that LOW is the voltage level
  }

  delay(200);
}
