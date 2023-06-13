
#include <time.h>
#include <ESP8266WiFi.h>
#include <Adafruit_AHTX0.h>

#ifndef APSSID
#define APSSID "FrogSpawn"
#define APPSK  "toadette&toad4eva"
#endif

const char* host = "esp8266-tls.thesebsite.com";
static const char serverCACert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDAjCCAeoCCQCaR/IZRVi9djANBgkqhkiG9w0BAQsFADBDMSMwIQYDVQQDDBpl
c3A4MjY2LXRscy50aGVzZWJzaXRlLmNvbTELMAkGA1UEBhMCVUsxDzANBgNVBAcM
BkxvbmRvbjAeFw0yMzA2MTMxNDIyNDJaFw0yNDA2MDMxNDIyNDJaMEMxIzAhBgNV
BAMMGmVzcDgyNjYtdGxzLnRoZXNlYnNpdGUuY29tMQswCQYDVQQGEwJVSzEPMA0G
A1UEBwwGTG9uZG9uMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2adq
nIYCUCaH63Xt6/kFX1eJ9GzeNEsH8HqmSrX4h1QhGI6oeNNOZUWJ5Y5hJKCnIxR5
QBXLnYHZtGI54qJG8Qq0QawdETCxrsOnt4EdPkJZ3MlhV39L1zWGJqXYI47xOxiI
V2vCC/23FO0QWfOWN4KKDBiVAOpWXutbgGOspvEpdeJEV+tWas9UdtT+mkVoLj1Y
BphUcPwITsujMHmGZDuYbaZDSzdM6rgpNvstNoqzSTHoe/D2F8VjB48jzeCrKP63
vYVT5fTPCqiu/aGuNye9Cc68P/X+ctO2Is3Xbf3ya+96t50noKYoEWpkEPUcRGmB
qd5TFMIqFlYOggYX4wIDAQABMA0GCSqGSIb3DQEBCwUAA4IBAQCa/UVLhx0YnAFL
f/aekSDaI5m891lkUpSkgfXVFaZag4ZTx8p0Dl2CiYD1JH+OmjfRpyVad/9z+i5o
sxqGgu/aqKEAlMN/RwOawneZGNrYy4uBBeoNzwrEQuWoglRwcRPbZ7H/yQMmJMo0
6FKV77HOU3ivP4eqCMqbcN96rEfD7dlxtCpOvpdKacPxUQTW7WeXF4jPY09Olj9Q
6eWHjUV1YpFd6gI0W1+4AvWqIkdMIHFNbaZK/6wBNxvycUvxL87P4OXeubBAsixx
lMdyOJtTsnqg5dHvcHkX4verVoY2lbgP7gzgwesB8fb+YeUsK1lXGxuE3SGO1vTL
9JX0Q0ci
-----END CERTIFICATE-----
)EOF";
// ^ USE ROOT CA NOT DOMAIN ONE

X509List x509(serverCACert);
WiFiClientSecure client;

Adafruit_AHTX0 aht;
Adafruit_Sensor *aht_humidity, *aht_temp;
int SOIL_MOISTURE = A0;
float soil_moisture_value = 0.0;

void setup(void) {
  Serial.begin(9600); // declare board rate (freq. of comms)
  Serial.setDebugOutput(true);
  Serial.println();

  // WIFI SETUP
  Serial.print("connecting to ");
  Serial.println(APSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(APSSID, APPSK);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Synchronize time useing SNTP. This is necessary to verify that
  // the TLS certificates offered by the server are currently valid.
  Serial.print("Setting time using SNTP");
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
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

  client.setTrustAnchors(&x509);

  // ADAFRUIT ENV SENSOR SETUP
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10/AHT20 chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("AHT10/AHT20 Found!");
  aht_temp = aht.getTemperatureSensor();
  aht_temp->printSensorDetails();

  aht_humidity = aht.getHumiditySensor();
  aht_humidity->printSensorDetails();

  // ANALOG SOIL MOISTURE SETUP 
  pinMode(SOIL_MOISTURE, OUTPUT);
}

void loop() {
  /* Get a new normalized sensor event */
  sensors_event_t humidity;
  sensors_event_t temp;
  aht_humidity->getEvent(&humidity);
  aht_temp->getEvent(&temp);

  /* Display the results (humidity is measured in % relative humidity (% rH) */
  Serial.print("\t\tHumidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println(" % rH");
  Serial.print("\t\tTemperature: ");
  Serial.print(temp.temperature);
  Serial.println(" degrees C");

  // Analog soil moisture reading
  soil_moisture_value = analogRead(SOIL_MOISTURE);
  Serial.println();
  Serial.print("SOIL MOISTURE = ");
  Serial.println(soil_moisture_value);

  // SEND DATA TO SERVER
  if (WiFi.status() == WL_CONNECTED) {
    // read data from server
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }

    Serial.print("\nStarting connection to host ");
    Serial.println(host);
    if (client.connect(host, 443))
      Serial.print("Connection success!");
    else {
      Serial.println("Connection failed!");
    }

    if (client.connected()) {
      // SEND DATA TO SERVER
      client.println("GET /?soil_moisture_value=" + String(soil_moisture_value) + " HTTP/1.1");
      client.println("Host: " + String(host));
      client.println();
    } else {
      Serial.println();
      Serial.println("Client no longer connected. Stopping.");
      client.stop();
      for(;;)
        ;
    }
  } 

  delay(5000);
}
