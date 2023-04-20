/*
  ESP8266 HTTPS experiments from ya boi Seb


  
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClientSecureBearSSL.h>

// TLS private key fingerprint
#define FINGERPRINT "57 A6 A3 FB 1B 76 3B 19 86 2C 2B A5 32 27 A8 C1 55 EA 43 F9 AD 60 E9 2D D1 62 3C 12 F6 5A 55 52"

ESP8266WiFiMulti WiFiMulti;

int LED = 5;
int LDR = A0;

float ldr_value = 0.0;

void setup() {
  pinMode(LED,OUTPUT);
  pinMode(LDR,INPUT);  

  Serial.begin(9600); // declare board rate (freq. of comms)

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("FrogSpawn", "2Boats1hose");
}

// the loop function runs over and over again forever
void loop() {
  // LOCAL LED CONTROLs
  ldr_value = analogRead(LDR);
  Serial.print("LDR = ");
  Serial.println(ldr_value);

  if (ldr_value < 600) {
    digitalWrite(LED, HIGH);  // Turn the LED on by making the voltage HIGH
  } else {
    digitalWrite(LED, LOW);  // Turn the LED off (Note that LOW is the voltage level)
  }

  // WIFI NETWORK STUFF
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

    client->setFingerprint(FINGERPRINT);
    // Or, if you happy to ignore the SSL certificate, then use the following line instead:
    // client->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://esp2866")) {  // HTTPS

      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // HTTP header has been send and Server response header has been handled
      // httpCode will be negative on error
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode > 0) {
    
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  }

  Serial.println("Wait 10s before next round...");
  delay(10000);
}
