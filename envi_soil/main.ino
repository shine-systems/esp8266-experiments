#include <vector>
#include <Adafruit_AHTX0.h>
#include <Shinode.h>

Adafruit_AHTX0 aht;
Adafruit_Sensor *aht_humidity, *aht_temp;
int SOIL_MOISTURE = A0;
float soil_moisture_value = 0.0;

void moistureSensorSetup() {
  pinMode(SOIL_MOISTURE, OUTPUT);
}

String moistureSensorSense() {
  float value = analogRead(SOIL_MOISTURE);
  int max = 700;
  int min = 200;
  float calibrated = ((value - min) / (max - min)) * 100;
  return String(calibrated);
}

Sensor moisture_sensor = {
  String("soil_moisture"),
  String("%"),
  moistureSensorSetup,
  moistureSensorSense
};

void climateSensorSetup() {
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
}

String climateSensorSense() {
  sensors_event_t humidity;
  sensors_event_t temp;
  aht_humidity->getEvent(&humidity);
  aht_temp->getEvent(&temp);

  return String("Humidity: " + String(humidity.relative_humidity) + "% rH" + " Temperature: " + String(temp.temperature) + " degrees C");
}

Sensor climate_sensor {
  String("climate"),
  String("humidity and temperature"),
  climateSensorSetup,
  climateSensorSense
};

Shinode device(
  "123",
  "super_secret_token",
  APSSID,
  APPSK,
  host,
  rootCACert,
  std::vector<Sensor>{ moisture_sensor, climate_sensor },
  std::vector<Controller>{}
);

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  Serial.println();
  
  device.connect();
}

void loop() {
  device.sync();
  delay(5000);
}