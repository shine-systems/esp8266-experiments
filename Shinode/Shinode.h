#ifndef Shinode_h
#define Shinode_h

#include <string>
#include <vector>
#include <tuple>
#include <time.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

using std::string;
using std::vector;
using std::tuple;

struct Result {
  string name;
  string unit;
  string value;
};

struct Sensor {
  string name;
  string unit;
  void (*setup)();
  Result (*sense)();
};

struct Controller {
  string name;
  string unit;
  void (*setup)();
  Result (*control)(Result);
};

class Shinode {
private:
  string device_id;
  string token;
  unsigned long last_poll;
  unsigned int polling_interval;
  WiFiClientSecure client;
  vector<Sensor> sensors;
  vector<Controller> controllers;

public:
  Shinode(
    string device_id,
    string token,
    string APSSID,
    string APPSK,
    string host,
    string rootCACert,
    vector<Sensor> sensors,
    vector<Controller> controllers
  ) : client(),
      device_id(device_id),
      token(token),
      last_poll(millis()),
      polling_interval(0),
      sensors(sensors),
      controllers(controllers) {
      
    X509List x509(rootCACert);
    client.setTrustAnchors(&x509);
    
    HTTPClient http;
    http.begin(client, host);
    http.addHeader("Authorization", "Bearer " + token);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      string payload = http.getString();

      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload);

      this->polling_interval = doc["polling_interval"];

      // Check if received sensor data matches Shinode config
      JsonArray receivedSensors = doc["sensors"];
      for (size_t i = 0; i < receivedSensors.size(); i++) {
        JsonObject receivedSensor = receivedSensors[i].as<JsonObject>();
        findSensorByName(receivedSensor["name"].as<string>());
      }

      // Check if received control data matches Shinode config
      JsonArray receivedControls = doc["controls"];
      for (size_t i = 0; i < receivedControls.size(); i++) {
        JsonObject receivedControl = receivedControls[i].as<JsonObject>();
        findControllerByName(receivedControl["name"].as<string>());
      }

      sense();
    }

    http.end();
  }

  tuple<Result*, size_t> sense() {
    size_t sensorCount = sensors.size();
    vector<Result> results(sensorCount);

    for (size_t i = 0; i < sensorCount; i++) {
      Sensor& sensor = sensors[i];
      Result result = sensor.sense();

      results[i] = result;
    }

    HTTPClient http;
    http.begin(client, "https://esp8266-tls.thewebsite.com/" + device_id + "/sense");
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(buildJsonPayload(results, sensorCount));

    if (httpCode == HTTP_CODE_OK) {
      string payload = http.getString();

      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload);

      size_t controlCount = doc.size();
      Result* actions = new Result[controlCount];

      for (size_t i = 0; i < controlCount; i++) {
        JsonObject control = doc[i].as<JsonObject>();
        string name = control["name"].as<string>();
        string unit = control["unit"].as<string>();
        string value = control["value"].as<string>();

        Result action = { name, unit, value };
        actions[i] = action;
      }

      http.end();
      return std::make_tuple(actions, controlCount);
    }

    http.end();
    return std::make_tuple(nullptr, 0);
  }

  void control(vector<Result>& actions) {
    size_t actionCount = actions.size();
    vector<Result> results(actionCount);

    for (size_t i = 0; i < actionCount; i++) {
      Result& action = actions[i];
      Controller controller = findControllerByName(action.name);
      Result result = controller.control(action);
      results[i] = result;
    }

    HTTPClient http;
    http.begin(client, "https://esp8266-tls.thewebsite.com/" + device_id + "/control");
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(buildJsonPayload(results, actionCount));

    http.end();
    delete[] results;
  }

  void sync() {
    if (polling_interval != 0 && millis() - last_poll > polling_interval) {
      vector<Result> actions = sense();
      control(actions);
    }
  }

private:
  Sensor findSensorByName(string name) {
    for (size_t i = 0; i < sensors.size(); i++) {
      Sensor& sensor = sensors[i];

      if (sensor.name == name) {
        return sensor;
      }
    }

    throw std::runtime_error("Sensor not found: " + name);
  }

  Controller findControllerByName(string name) {
    for (size_t i = 0; i < controllers.size(); i++) {
      Controller& controller = controllers[i];

      if (controller.name == name) {
        return controller;
      }
    }

    throw std::runtime_error("Controller not found: " + name);
  }

  string buildJsonPayload(Result* results, size_t resultCount) {
    DynamicJsonDocument doc(200);
    JsonArray jsonArray = doc.to<JsonArray>();

    for (size_t i = 0; i < resultCount; i++) {
      Result result = results[i];
      JsonObject jsonResult = jsonArray.createNestedObject();

      jsonResult["name"] = result.name;
      jsonResult["unit"] = result.unit;
      jsonResult["value"] = result.value;
    }

    return doc.as<String>();
  }
};

#endif