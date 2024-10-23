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