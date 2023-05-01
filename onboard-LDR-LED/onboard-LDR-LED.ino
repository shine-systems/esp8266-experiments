/*
  ESP8266 HTTPS experiments from ya boi Seb

 * Sources:
 * https://www.mischianti.org/2022/01/09/esp8266-self-ota-update-in-https-ssl-tls-with-trusted-self-signed-certificate-3/#Modify_sketch_to_do_a_secure_HTTPs_call
 * https://randomnerdtutorials.com/esp32-esp8266-https-ssl-tls/#esp8266-https-requests
 */
 
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <time.h>

#ifndef APSSID
#define APSSID "Frameworks Westminster"
#define APPSK  "w3stminst3r"
#endif

static const char serverCACert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDAjCCAeoCCQCLTw9BomNgHjANBgkqhkiG9w0BAQsFADBDMSMwIQYDVQQDDBpl
c3A4MjY2LXRscy50aGVzZWJzaXRlLmNvbTELMAkGA1UEBhMCVUsxDzANBgNVBAcM
BkxvbmRvbjAeFw0yMzAzMzExODIxNTZaFw0yNDAzMjExODIxNTZaMEMxIzAhBgNV
BAMMGmVzcDgyNjYtdGxzLnRoZXNlYnNpdGUuY29tMQswCQYDVQQGEwJVSzEPMA0G
A1UEBwwGTG9uZG9uMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsZd6
TxL/rt+PiVIAkg1d9SVxCHNp0TvDVwb2sP6aZbFiKZ+U2eUfUJnVroeDKhLH57Cb
NoWiUcdHGxvRbxZU55fewi9211zrNyLx6ViSGp9Gd0X3haQjJDtWJg7S3vPCmsF9
WDq+skVF9ijlj9KWwqi2ARz2xxqFMQiRmXa4me20/zcN7Ee1ZjkbGt/1cW8IccHl
RSBTVVRzEyVddbv8xP66rexACOkhaqW98s49qFd3AbxoIlkRdw5tL3azW7BKk4NB
WFdxqNx2oRMn3L+I+SsWW4nmV6wHVMEhqyYQmlW/Z8wXXhClysUIbf0eFKt1OyHX
tSRPl6AHxBg6vHpajwIDAQABMA0GCSqGSIb3DQEBCwUAA4IBAQBcDS27689EjWe+
fYAe1ILiMU+bHr1ezx0B+nxYhinzrn3TqobALEnRz14N/gbsIgRKpsk5WhG68PMB
E+yMZoWR2xzXIWqZ7lFCroEhWfNt8gRGBAHvbc52iHV72LuNI1QlvLCqNa0s0Aw9
jhUep+ddUpU4K9XGOcwo3djtf1WB7asB0LGb7iM+/+IfNI640By+SgHo9jPPHHlF
+7M2yOmvavSSfuoX/SqWLn5I6d+mofCvboKBNgTm2GoVbFt4WLZbw6KY+woqOtKa
XNOxCHVC8Eb5CBOWaKILj87vWwOQz+QYbXu4UJOr1ne7gAhl52NlxdXHe6uqYb0z
K9JlFV3s
-----END CERTIFICATE-----
)EOF";

ESP8266WiFiMulti WiFiMulti;
BearSSL::WiFiClientSecure client;
BearSSL::X509List x509(serverCACert);

// Set time via NTP, as required for x.509 validation
time_t setClock() {
  configTime(2*3600, 0, "pool.ntp.org", "time.nist.gov");
 
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
  return now;
}

const char* server = "esp8266-tls.thesebsite.com";
int LED = 5;
int LDR = A0;
float ldr_value = 0.0;

void setup() {
  Serial.begin(9600); // declare board rate (freq. of comms)
  Serial.setDebugOutput(true);
  pinMode(LED,OUTPUT);
  pinMode(LDR,INPUT); 

  // client.setTrustAnchors(&x509);
  setClock(); 
  // client.allowSelfSignedCerts();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(APSSID, APPSK);
 
  Serial.println();
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  // wait for WiFi connection
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nStarting connection to server...");
  if (client.connect(server, 443))
    Serial.println("Connected to server!");
  else {
    Serial.println("Connection failed!");
  }
}
 
void loop() {
  ldr_value = analogRead(LDR);
  Serial.println();
  Serial.print("LDR = ");
  Serial.println(ldr_value);

  if ((WiFiMulti.run() == WL_CONNECTED)) {
    // read data from server
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }

    if (client.connected()) {
      // SEND DATA TO SERVER
      client.println("GET /?ldr_value=" + String(ldr_value) + " HTTP/1.1");
      client.println("Host: " + String(server));
      client.println();
    } else {
      Serial.println();
      Serial.println("Client no longer connected. Stopping.");
      client.stop();
      for(;;)
        ;
    }
  } 

  // local LED controls
  if (ldr_value < 600) {
    digitalWrite(LED, HIGH);  // Turn the LED on by making the voltage HIGH
  } else {
    digitalWrite(LED, LOW);  // Turn the LED off (Note that LOW is the voltage level)
  }

  // Serial.println("Wait 1s before next round...");
  delay(1000);
}
