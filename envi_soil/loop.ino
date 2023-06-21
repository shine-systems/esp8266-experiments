int SOIL_MOISTURE = A0;
float soil_moisture_value = 0.0;

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
      //const char url = "/?soil_moisture_value="
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