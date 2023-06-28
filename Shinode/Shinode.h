/*
  Shinode class for use on Arduino-based microcontrollers (such as ESP8266)
  to connect to the Shineponics cloud for smart farming.
  Created by Seb Ringrose (github.com/sejori), 28 Jun 2023.
  See repository license for licensing information.
*/
#ifndef Shinode_h
#define Shinode_h

#include <vector>
#include <tuple>
#include <time.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

using std::vector;

struct Result {
  String name;
  String unit;
  String value;
};

struct Sensor {
  String name;
  String unit;
  void (*setup)();
  String (*sense)();
};

struct Controller {
  String name;
  String unit;
  void (*setup)();
  String (*control)(Result);
};

class Shinode {
private:
  String device_id;
  String token;
  unsigned long last_poll;
  unsigned int polling_interval;
  WiFiClientSecure client;
  vector<Sensor> sensors;
  vector<Controller> controllers;

public:
  Shinode(
    String device_id,
    String token,
    String APSSID,
    String APPSK,
    String host,
    String rootCACert,
    vector<Sensor> sensors,
    vector<Controller> controllers
  ) : client(),
      device_id(device_id),
      token(token),
      last_poll(millis()),
      polling_interval(0),
      sensors(sensors),
      controllers(controllers) {

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
      
    X509List x509(rootCACert.c_str());
    client.setTrustAnchors(&x509);
    
    HTTPClient http;
    http.begin(client, host);
    http.addHeader("Authorization", "Bearer " + token);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload);

      this->polling_interval = doc["polling_interval"];

      // Check if received sensor data matches Shinode config
      JsonArray receivedSensors = doc["sensors"];
      for (size_t i = 0; i < receivedSensors.size(); i++) {
        JsonObject receivedSensor = receivedSensors[i];
        findSensorByName(receivedSensor["name"]);
      }

      // Check if received control data matches Shinode config
      JsonArray receivedControls = doc["controls"];
      for (size_t i = 0; i < receivedControls.size(); i++) {
        JsonObject receivedControl = receivedControls[i];
        findControllerByName(receivedControl["name"]);
      }

      sense();
    } else {
      Serial.println();
      Serial.print("Bad response from server in Shinode constructor: " + device_id);
    }

    http.end();
  }

  vector<Result> sense() {
    size_t sensorCount = sensors.size();
    vector<Result> results(sensorCount);

    for (size_t i = 0; i < sensorCount; i++) {
      Sensor& sensor = sensors[i];
      Result result = {
        sensor.name,
        sensor.unit,
        sensor.sense()
      };

      results[i] = result;
    }

    HTTPClient http;
    http.begin(client, String(host + "/" + String(device_id) + "/sense"));
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(buildJsonPayload(results));
    vector<Result> actions;

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload);
      size_t controlCount = doc.size();

      for (size_t i = 0; i < controlCount; i++) {
        JsonObject control = doc[i];
        String name = control["name"];
        String unit = control["unit"];
        String value = control["value"];

        Result action = { name, unit, value };
        actions[i] = action;
      }

      http.end();
      return actions;
    } else {
      Serial.println();
      Serial.print("Bad response from server in Shinode sense: " + device_id);
    }

    http.end();
    return actions;
  }

  void control(vector<Result>& actions) {
    size_t actionCount = actions.size();
    vector<Result> results(actionCount);

    for (size_t i = 0; i < actionCount; i++) {
      Result& action = actions[i];
      Controller controller = findControllerByName(action.name);
      Result result = {
        controller.name,
        controller.unit,
        controller.control(action)
      };
      results[i] = result;
    }

    HTTPClient http;
    http.begin(client, String(host + "/" +  String(device_id) + "/control"));
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(buildJsonPayload(results));
    if (httpCode != HTTP_CODE_OK) {
      Serial.println();
      Serial.print("Bad response from server in Shinode control: " + device_id);
    }

    http.end();
  }

  void sync() {
    if (polling_interval != 0 && millis() - last_poll > polling_interval) {
      vector<Result> actions = sense();
      control(actions);
    }
  }

private:
  Sensor findSensorByName(String name) {
    for (size_t i = 0; i < sensors.size(); i++) {
      Sensor& sensor = sensors[i];

      if (sensor.name == name) {
        return sensor;
      }
    }

    Serial.println();
    Serial.print("Sensor config not found: " + name);
  }

  Controller findControllerByName(String name) {
    for (size_t i = 0; i < controllers.size(); i++) {
      Controller& controller = controllers[i];

      if (controller.name == name) {
        return controller;
      }
    }

    Serial.println();
    Serial.print("Controller config not found: " + name);
  }

  String buildJsonPayload(vector<Result> results) {
    size_t capacity = JSON_ARRAY_SIZE(results.size()) + results.size() * JSON_OBJECT_SIZE(3);
    DynamicJsonDocument doc(capacity);
    JsonObject obj = doc.to<JsonObject>();

    for (size_t i = 0; i < results.size(); i++) {
      Result result = results[i];
      obj["name"] = result.name;
      obj["unit"] = result.unit;
      obj["value"] = result.value;
    }

    String json;
    serializeJson(doc, json);

    return json;
  }
};

#endif