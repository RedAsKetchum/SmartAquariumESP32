float temperatureMin;
float temperatureMax;
float turbidityMin;
float turbidityMax;
float pHMin;
float pHMax;

void loop() {
  unsigned long currentMillis = millis();

  // Keep the connection to Adafruit IO alive
  io.run();

  // Ensure we are connected to Adafruit IO using MQTT //Handle reconnection 
  MQTT_connect();

  // Process any incoming messages from Adafruit IO
  Adafruit_MQTT_Subscribe *subscription;

  while ((subscription = mqtt.readSubscription(100))) {
    // Handle color feed subscription
    if (subscription == &colorFeed) {
      char *controlData = (char *)colorFeed.lastread;
      Serial.print("Control data received: ");
      Serial.println(controlData);

      // Check if the payload is "ON" or "OFF"
      if (strcmp(controlData, "ON") == 0) {
        handleOnOff("ON");
      } else if (strcmp(controlData, "OFF") == 0) {
        handleOnOff("OFF");
      } else {
        // If it's not "ON" or "OFF", treat it as RGB and brightness values
        handleColorData(controlData);
      }
    }

    // Handle schedule feed subscription
    if (subscription == &scheduleFeed) {
      char *scheduleData = (char *)scheduleFeed.lastread;
      Serial.print("Raw Schedule Data Received: ");
      Serial.println(scheduleData);

      handleScheduleData(scheduleData);
      printAllSchedules();
    }

    // Handle servo feed subscription - MODIFY HERE!
    // if (subscription == &servoFeed) {
    //   String data = (char *)servoFeed.lastread;
    //   if (data == "activate") {
    //     activateServo();  // Call the function to activate the servo
    //   }
    // }

 // Handle servo feed subscription
if (subscription == &servoFeed) {
    String data = (char *)servoFeed.lastread;
    Serial.print("Received data: ");
    Serial.println(data);  // Debugging print for the raw data

    // Create a JSON document to parse the data
    StaticJsonDocument<200> doc;

    // Parse the JSON string
    DeserializationError error = deserializeJson(doc, data);

    // Check if parsing succeeded
    if (!error) {
        // Print entire JSON data for debugging
        serializeJson(doc, Serial);
        Serial.println();

        // Extract the "action" key
        const char* action = doc["action"];
        Serial.print("Action received: ");
        Serial.println(action);  // Debug print for action value

        // Extract the "Amount" and "scheduledDispenses" keys
        int manualValue = doc["Amount"] | -1;  // Use -1 as fallback for manual dispensing
        int scheduledDispenses = doc["scheduledDispenses"] | -1;  // Use -1 as fallback for scheduled dispensing

        // Debug print for Amount and scheduledDispenses values
        Serial.print("Manual Amount received: ");
        Serial.println(manualValue);
        
        // Check if the action is "activate" for manual dispensing
        if (String(action) == "activate" && manualValue > 0) {
            Serial.println("Starting manual dispensing cycles...");

            // Loop for the specified number of manual dispense cycles
            for (int i = 0; i < manualValue; i++) {
                Serial.print("Manual Dispense cycle: ");
                Serial.println(i + 1);

                // Start the servo movement
                activateServo();

                // Wait until the servo has completed its movement cycle
                while (servoActive) {
                    handleServoMovement();
                    delay(10);  // Small delay to prevent overloading the loop
                }

                // Short delay between dispense cycles
                delay(1000);
            }
            Serial.println("Manual dispensing completed.");

        // Check if this is a scheduled dispensing request
        } else if (scheduledDispenses > 0) {
            Serial.println("Starting scheduled dispensing cycles...");

            // Loop for the specified number of scheduled dispense cycles
            for (int i = 0; i < scheduledDispenses; i++) {
                Serial.print("Scheduled Dispense cycle: ");
                Serial.println(i + 1);

                // Start the servo movement
                activateServo();

                // Wait until the servo has completed its movement cycle
                while (servoActive) {
                    handleServoMovement();
                    delay(10);  // Small delay to prevent overloading the loop
                }

                // Short delay between dispense cycles
                delay(1000);
            }
            Serial.println("Scheduled dispensing completed.");
        } else {
            Serial.println("Invalid or missing Amount or scheduledDispenses, no dispensing.");
        }
    } else {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
    }
}
    //Reads the sensor settings user set limits
    if (subscription == &sensorSettingsFeed) { 
          Serial.print("Received sensor settings data: ");
          String jsonString = (char *)sensorSettingsFeed.lastread;
          Serial.println(jsonString);

          // Parse the JSON string
          DynamicJsonDocument doc(1024); // Adjust size as needed
          DeserializationError error = deserializeJson(doc, jsonString);

          // Check for errors in parsing
          if (error) {
            Serial.print("Failed to parse JSON: ");
            Serial.println(error.f_str());
            return;
          }

          // Retrieve values from the JSON object
          temperatureMin = doc["temperatureMin"];
          temperatureMax = doc["temperatureMax"];
          //turbidityMin = doc["turbidityMin"];
          turbidityMax = doc["turbidityMax"];
          pHMin = doc["pHMin"];
          pHMax = doc["pHMax"];

          // Print the received values
          Serial.println("Temperature Min: " + String(temperatureMin));
          Serial.println("Temperature Max: " + String(temperatureMax));
          //Serial.println("Turbidity Min: " + String(turbidityMin));
          Serial.println("Turbidity Max: " + String(turbidityMax));
          Serial.println("pH Min: " + String(pHMin));
          Serial.println("pH Max: " + String(pHMax));
        }
  }

  // Reset executed flags if a new day has started
  resetExecutedFlagsIfNewDay();
  monitorScheduleChanges();
 
  // Check and control LEDs based on the schedule
  if (scheduleCount > 0) {
    checkScheduleAndControlDevices();
  }

  // Periodically fetch schedules to update IDs every 60 seconds
  static unsigned long lastFetch = 0;
  if (currentMillis - lastFetch > 60000) {
    fetchSchedulesFromAdafruitIO();
    lastFetch = currentMillis;
  }

  // Check if the interval has passed before inserting a new entry in the database
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    
     if (wifiNetworkFeed.publish(jsonString.c_str())) {
      //Serial.println("Wi-Fi network name sent to Adafruit IO");
    } else {
      Serial.println("Failed to send Wi-Fi network name");
    }

    // Count the number of entries in the database
    const char* sqlCount = "SELECT COUNT(*) FROM SensorData;";
    sqlite3_stmt* stmt;
    int rowCount = 0;
    
    rc = sqlite3_prepare_v2(db, sqlCount, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW) {
        rowCount = sqlite3_column_int(stmt, 0);  // Get the number of rows
      }
    }
    sqlite3_finalize(stmt);

    // If the table has 24 entries, delete the oldest one
    if (rowCount >= 24) {
      // Find the entry with the smallest ID (oldest entry)
      const char* sqlSelectOldest = "SELECT MIN(ID) FROM SensorData;";
      int oldestId = 0;
      rc = sqlite3_prepare_v2(db, sqlSelectOldest, -1, &stmt, NULL);
      if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          oldestId = sqlite3_column_int(stmt, 0);  // Get the ID of the oldest row
        }
      }
      sqlite3_finalize(stmt);

      Serial.printf("Deleting entry with ID: %d\n", oldestId);

      // Delete the oldest entry
      String sqlDeleteOldest = "DELETE FROM SensorData WHERE ID = " + String(oldestId) + ";";
      rc = sqlite3_exec(db, sqlDeleteOldest.c_str(), 0, 0, &zErrMsg);
      if (rc != SQLITE_OK) {
        Serial.printf("SQL error during deletion: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);  // Free memory for error message
      } else {
        Serial.println(F("Oldest entry deleted successfully."));
      }
    }

    // Read Temperature sensor value 
    tempSensor.requestTemperatures();
    float temperatureC = tempSensor.getTempCByIndex(0);
    float temperatureF = tempSensor.toFahrenheit(temperatureC);
    String sensor1Timestamp = getTimestamp();

    // Read the analog value from the pH sensor (10-bit ADC: 0-4095)
    pHArray[pHArrayIndex++] = analogRead(PH_SENSOR_PIN);
    if (pHArrayIndex == ArrayLenth) pHArrayIndex = 0;
    voltage = avergearray(pHArray, ArrayLenth) * 3.3 / 4095;
    currentPH = 3.5 * voltage + Offset;
    pHValue =  currentPH;
    String sensor2Timestamp = getTimestamp();

    // Read the turbidity sensor's output data voltage
    int currentTurbidity = analogRead(turbidityPin);
    float turbidityVoltage = currentTurbidity * (3.3 / 4095.0); // Convert ADC reading to voltage (3.3V reference)
    float turbidityValue = turbidityVoltage;
    String sensor3Timestamp = getTimestamp();
  
    // Print raw voltage and calculated NTU value
    //Serial.print("Raw Voltage: ");
    //Serial.println(turbidityVoltage);

    // Random value for another sensor
    //float turbidityValue = random(0, 10000) / 100.0;
    //String sensor3Timestamp = getTimestamp();

    // Format sensor values to 2 decimal places
    String formattedSensor1 = formatValue(temperatureF);
    String formattedSensor2 = formatValue(pHValue);
    String formattedSensor3 = formatValue(turbidityValue);

    // Get the current date formatted as MM.DD
    String formattedDate = getFormattedDate();

    // Insert the sensor data and the formatted date into the SQLite database
    String sqlInsert = "INSERT INTO SensorData (Sensor1, Sensor1Timestamp, Sensor2, Sensor2Timestamp, Sensor3, Sensor3Timestamp, Date) VALUES (" +
                       formattedSensor1 + ", '" + sensor1Timestamp + "', " + 
                       formattedSensor2 + ", '" + sensor2Timestamp + "', " + 
                       formattedSensor3 + ", '" + sensor3Timestamp + "', '" + 
                       formattedDate + "');";

    rc = sqlite3_exec(db, sqlInsert.c_str(), 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
      Serial.printf("SQL error during data insertion: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);  // Free memory for error message
    }

    // Fetch the newest entry as JSON and print it
    String jsonResponse = fetchNewestEntryAsJson();
    const char* jsonString = jsonResponse.c_str();

    // Publish the JSON string to Adafruit IO
    if (!sensorDataFeed.publish(jsonString)) {
      Serial.println(F("Failed to send sensor data to Adafruit IO."));
    } else {
      //Serial.println(F("Sensor data sent to Adafruit IO..."));
    }
    
    //Display sensor's json data
    //Serial.println(jsonResponse);

    //Check if sensor values exceed thresholds and notify the user. Limits set by the user.
    checkSensorValues(temperatureF, pHValue, turbidityValue); 
    
    // Print the table after the insertion or update
    //printTable();

  }

  // Handle servo movement asynchronously
  handleServoMovement();

  // Check if Wi-Fi is connected, and reconnect if necessary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi not connected! Attempting to reconnect..."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  //delay(2000);  // Short delay to avoid flooding serial output
}

