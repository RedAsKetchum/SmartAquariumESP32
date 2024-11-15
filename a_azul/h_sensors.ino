// Function to get the current Sensor1 value
float getCurrentSensor1Value() {
  const char* sqlSelectNewestEntry = "SELECT Sensor1 FROM SensorData ORDER BY ID DESC LIMIT 1;";
  sqlite3_stmt* stmt;
  float sensor1Value = 0;

  rc = sqlite3_prepare_v2(db, sqlSelectNewestEntry, -1, &stmt, NULL);
  if (rc == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      sensor1Value = sqlite3_column_double(stmt, 0);  // Get the Sensor1 value
    }
  }
  sqlite3_finalize(stmt);
  return sensor1Value;
}

double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to averaging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}

String formatValue(float value) {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%.2f", value);  // Format with 2 decimal places
  return String(buffer);
}

// Function to print the contents of the SensorData table
void printTable() {
  const char* sqlSelect = "SELECT * FROM SensorData;";
  sqlite3_stmt* stmt;
  
  // Prepare the SQL query
  rc = sqlite3_prepare_v2(db, sqlSelect, -1, &stmt, NULL);
  
  if (rc != SQLITE_OK) {
    Serial.printf("Failed to execute query: %s\n", sqlite3_errmsg(db));
    return;
  }

  // Print a header for the table
  Serial.println("------ SensorData Table ------");
  Serial.println("ID | Temperature Sensor1 | Timestamp  | pH Sensor2 | Timestamp  | Turbidity Sensor3 | Timestamp | Date");

  // Fetch each row and print the data
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    float sensor1 = sqlite3_column_double(stmt, 1);
    const unsigned char* sensor1Timestamp = sqlite3_column_text(stmt, 2);
    float sensor2 = sqlite3_column_double(stmt, 3);
    const unsigned char* sensor2Timestamp = sqlite3_column_text(stmt, 4);
    float sensor3 = sqlite3_column_double(stmt, 5);
    const unsigned char* sensor3Timestamp = sqlite3_column_text(stmt, 6);


    const unsigned char* date = sqlite3_column_text(stmt, 7);  // Get the Date column
    
    //Prints Table on serial monitor
    Serial.printf("%d  | %.2f   | %s   | %.2f   | %s   | %.2f   | %s   |  %s\n", 
                  id, sensor1, sensor1Timestamp, sensor2, sensor2Timestamp, sensor3, sensor3Timestamp, date);
  }
  
  // Finalize the statement
  sqlite3_finalize(stmt);
  Serial.println("------------------------------");
}

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buffer[9];  // Adjust buffer size for just the time
  strftime(buffer, sizeof(buffer), "%I:%M %p", timeInfo);  // Format only the time
  return String(buffer);
}

//Function to get the current Date in format MM.DD
String getFormattedDate() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buffer[6];  // Buffer to hold the formatted date
  strftime(buffer, sizeof(buffer), "%m.%d", timeInfo);  // Format the date as MM.DD
  return String(buffer);
}

// Function to configure and synchronize time with NTP server
void setupTime() {
  configTime(-14400, 0, "pool.ntp.org", "time.nist.gov");  // Set NTP servers
  Serial.println("Waiting for time synchronization...");
  
  // Wait until the time is synchronized (wait for a non-default time)
  while (time(nullptr) < 8 * 3600 * 2) {
    Serial.print("...");
    delay(500);
  }
  Serial.println("\nTime synchronized");
}

String fetchNewestEntryAsJson() {
  const char* sqlSelectNewestEntry = 
    "SELECT ID, Sensor1, Sensor1Timestamp, Sensor2, Sensor2Timestamp, Sensor3, Sensor3Timestamp, Date "
    "FROM SensorData ORDER BY ID DESC LIMIT 1;";  // Query to fetch the newest entry
  
  sqlite3_stmt* stmt;
  String jsonResult = "{";

  // Prepare the SQL query
  rc = sqlite3_prepare_v2(db, sqlSelectNewestEntry, -1, &stmt, NULL);

  if (rc != SQLITE_OK) {
    Serial.printf("Failed to execute query: %s\n", sqlite3_errmsg(db));
    return "{}";  // Return empty JSON if there's an error
  }

  // Fetch and prepare the JSON response
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    float sensor1 = sqlite3_column_double(stmt, 1);
    const unsigned char* sensor1Timestamp = sqlite3_column_text(stmt, 2);
    float sensor2 = sqlite3_column_double(stmt, 3);
    const unsigned char* sensor2Timestamp = sqlite3_column_text(stmt, 4);
    float sensor3 = sqlite3_column_double(stmt, 5);
    const unsigned char* sensor3Timestamp = sqlite3_column_text(stmt, 6);
    const unsigned char* date = sqlite3_column_text(stmt, 7);
 
    // Format the JSON data
    jsonResult += "\"ID\":" + String(id) + ",";
    jsonResult += "\"Sensor1\":" + formatValue(sensor1) + ",";
    jsonResult += "\"Sensor1Timestamp\":\"" + String((char*)sensor1Timestamp) + "\",";
    jsonResult += "\"Sensor2\":" + formatValue(sensor2) + ",";
    jsonResult += "\"Sensor2Timestamp\":\"" + String((char*)sensor2Timestamp) + "\",";
    jsonResult += "\"Sensor3\":" + formatValue(sensor3) + ",";
    jsonResult += "\"Sensor3Timestamp\":\"" + String((char*)sensor3Timestamp) + "\",";
    jsonResult += "\"Date\":\"" + String((char*)date) + "\"";  // Add the Date field to the JSON

  } else {
    return "{}";  // Return empty JSON if no Fdata is found
  }

  jsonResult += "}";
  
  // Finalize the statement
  sqlite3_finalize(stmt);
  return jsonResult;
}

 // Function to check sensor values against thresholds
  void checkSensorValues(float temperatureF, float pHValue, float turbidityValue) {
      // Check if temperature is below minimum
      if (temperatureF < temperatureMin) {
          // Send alert for low temperature
          if (notifications.publish("low temperature")) {
              //Serial.println("Alert sent for low temperature");
          } else {
              Serial.println("Failed to send alert for low temperature");
          }
      }
      // Check if temperature is above maximum
      if (temperatureF > temperatureMax) {
          // Send alert for high temperature
          if (notifications.publish("high temperature")) {
              //Serial.println("Alert sent for high temperature");
          } else {
              Serial.println("Failed to send alert for high temperature");
          }
      }
      // Check if pH is below minimum
      if (pHValue < pHMin) {
          // Send alert for low pH
          if (notifications.publish("low pH")) {
              //Serial.println("Alert sent for low pH");
          } else {
              Serial.println("Failed to send alert for low pH");
          }
      }
      // Check if pH is above maximum
      if (pHValue > pHMax) {
          // Send alert for high pH
          if (notifications.publish("high pH")) {
              //Serial.println("Alert sent for high pH");
          } else {
              Serial.println("Failed to send alert for high pH");
          }
      }
      // // Check if turbidity is below minimum
      // if (turbidityValue < turbidityMin) {
      //     // Send alert for low turbidity
      //     if (notifications.publish("low turbidity")) { //this doesnt make sense
      //         //Serial.println("Alert sent for low turbidity");
      //     } else {
      //         Serial.println("Failed to send alert for low turbidity");
      //     }
      // }
      // Check if turbidity is above maximum
      if (turbidityValue > turbidityMax) {
          // Send alert for high turbidity
          if (notifications.publish("high turbidity")) {
              //Serial.println("Alert sent for high turbidity");
          } else {
              Serial.println("Failed to send alert for high turbidity");
          }
      }
  }


