/*
  Shinode class for use on Arduino-based microcontrollers (such as ESP8266)
  to connect to the Shineponics cloud for smart farming.
  Created by Seb Ringrose (github.com/sejori), 28 Jun 2023.
  See repository license for licensing information.
*/
#ifndef Shinode_h
#define Shinode_h

#include <vector>
#include <functional>
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
  std::function<void()> setup;
  std::function<String()> sense;
};

struct Controller {
  String name;
  String unit;
  std::function<void()> setup;
  std::function<String(Result)> control;
};

class Shinode {
private:
  const char* device_id;
  const char* token;
  const char* APSSID;
  const char* APPSK;
  const char* host;
  const char* rootCACert;
  unsigned long last_poll;
  unsigned int polling_interval;
  WiFiClientSecure client;
  HTTPClient http;
  vector<Sensor> sensors;
  vector<Controller> controllers;

public:
  Shinode(
    const char* device_id,
    const char* token,
    const char* APSSID,
    const char* APPSK,
    const char* host,
    const char* rootCACert,
    vector<Sensor> sensors,
    vector<Controller> controllers
  ) : client(),
      device_id(device_id),
      token(token),
      APSSID(APSSID),
      APPSK(APPSK),
      host(host),
      rootCACert(rootCACert),
      last_poll(millis()),
      polling_interval(0),
      sensors(sensors),
      controllers(controllers) {}

  void connect() {
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
    Serial.print("WiFi connected");
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
      
    X509List cert(rootCACert);
    client.setTrustAnchors(&cert);

    Serial.println("Connecting to host: " + String(host));
    http.begin(client, host, 443, "/" + String(device_id) + "/connect", true);
    http.addHeader("Authorization", "Bearer " + String(token));

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Connection success.");
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      polling_interval = doc["polling_interval"];

      // setup sensors
      size_t sensorCount = sensors.size();
      for (size_t i = 0; i < sensorCount; i++) {
        Sensor sensor = sensors[i];
        Serial.println("Setting up " + sensor.name);
        sensor.setup();
      }

      // Check if received sensor data matches Shinode config
      JsonArray receivedSensors = doc["sensors"];
      if (receivedSensors) {
        for (size_t i = 0; i < receivedSensors.size(); i++) {
          JsonObject receivedSensor = receivedSensors[i];
          Serial.println(receivedSensor);
          findSensorByName(receivedSensor["name"]);
        }

        // Check if received control data matches Shinode config
        JsonArray receivedControls = doc["controls"];
        for (size_t i = 0; i < receivedControls.size(); i++) {
          JsonObject receivedControl = receivedControls[i];
          findControllerByName(receivedControl["name"]);
        }
      } else {
        Serial.println("No sensor data received for Shinode id: " + String(device_id));
      }
    } else {
      Serial.println("Bad response (" + String(httpCode) + ") in connect for Shinode id: " + device_id);
    }

    http.end();
  }

  vector<Result> sense() {
    Serial.println("sense called");
    size_t sensorCount = sensors.size();
    vector<Result> results(sensorCount);
    vector<Result> actions(0);
    Serial.println("Number of sensors registered locally: " + String(sensorCount));

    for (size_t i = 0; i < sensorCount; i++) {
      Sensor sensor = sensors[i];
      Serial.print("Sensing " + sensor.name);
      String value = sensor.sense();
      Serial.println(" value is " + value);
      Result result = {
        sensor.name,
        sensor.unit,
        value
      };

      results[i] = result;
    }

    http.begin(client, host, 443, "/" + String(device_id) + "/sense", true);
    http.addHeader("Authorization", "Bearer " + String(token));
    http.addHeader("Content-Type", "application/json");

    // POST sense data and set responded actions
    int httpCode = http.POST(buildJsonPayload(results));
    Serial.println(httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Actions from server:");
      Serial.println(payload);
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      size_t controlCount = doc.size();
      actions.resize(controlCount);

      for (size_t i = 0; i < controlCount; i++) {
        JsonObject control = doc[i];
        String name = control["name"];
        String unit = control["unit"];
        String value = control["value"];

        Result action = { name, unit, value };
        actions[i] = action;
      }
    } else {
      Serial.println("Bad response (" + String(httpCode) + ") in sense for Shinode id: " + String(device_id));
    }
          
    http.end();
    return actions;
  }

  void control(vector<Result> actions) {
    Serial.println("control called");
    size_t actionCount = actions.size();
    if (actions[0].name == "null") {
      Serial.println("No actions given to control.");
      return;
    }

    vector<Result> results(actionCount);

    for (size_t i = 0; i < actionCount; i++) {
      Result action = actions[i];
      Controller* controller = findControllerByName(action.name);
      if (controller) {
        Result result = {
          controller->name,
          controller->unit,
          controller->control(action)
        };
        results[i] = result;
      }
    }

    http.begin(client, host, 443, "/" + String(device_id) + "/control", true);
    http.addHeader("Authorization", "Bearer " + String(token));
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(buildJsonPayload(results));
    if (httpCode != HTTP_CODE_OK) {
      Serial.println();
      Serial.print("Bad response (" + String(httpCode) + ") in control for Shinode id: " + String(device_id));
    }

    http.end();
  }

  void sync() {
    Serial.println("sync called");
    if (polling_interval != 0 && millis() - last_poll > polling_interval) {
      Serial.println("passed sync condition");
      vector<Result> actions = sense();
      control(actions);
    }
  }

private:
  Sensor* findSensorByName(String name) {
    for (size_t i = 0; i < sensors.size(); i++) {
      Sensor* sensor = &sensors[i];
      if (sensor->name == name) {
        Serial.println(name);
        return sensor;
      }
    }

    Serial.println();
    Serial.print("Sensor config not found: " + name);
    return nullptr;
  }

  Controller* findControllerByName(String name) {
    for (size_t i = 0; i < controllers.size(); i++) {
      Controller* controller = &controllers[i];
      if (controller->name == name) {
        return controller;
      }
    }

    Serial.println();
    Serial.print("Controller config not found: " + name);
    return nullptr;
  }

  String buildJsonPayload(vector<Result> results) {
    size_t capacity = JSON_ARRAY_SIZE(results.size()) + results.size() * JSON_OBJECT_SIZE(3);
    DynamicJsonDocument doc(capacity);
    Serial.println(results.size());

    for (size_t i = 0; i < results.size(); i++) {
      Result result = results[i];
      doc[i]["name"] = result.name;
      doc[i]["unit"] = result.unit;
      doc[i]["value"] = result.value;
    }

    String json;
    serializeJson(doc, json);

    Serial.println(json);

    return json;
  }
};

#endif