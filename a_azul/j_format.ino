void handleFormatCommand(AdafruitIO_Data *data) {
  Serial.println("Message received from Adafruit IO:");

  if (data) {
    String command = data->toString();
    Serial.println("Command: " + command);

    if (command == "format") {
      Serial.println("Format command received. Formatting SPIFFS...");

      // Perform SPIFFS format
      if (SPIFFS.format()) {
        Serial.println("SPIFFS formatted successfully.");

        // Re-mount SPIFFS after formatting
        if (SPIFFS.begin()) {
          Serial.println("SPIFFS re-mounted successfully.");

          // Proceed to create or initialize the database
          initializeDatabase();  // This should create tables if they don’t exist
        } else {
          Serial.println("Failed to re-mount SPIFFS after formatting.");
        }
      } else {
        Serial.println("SPIFFS format failed.");
      }
    } else {
      Serial.println("Unknown command received: " + command);
    }
  } else {
    Serial.println("No data received from Adafruit IO.");
  }
}

void initializeDatabase() {
  // Ensure the database file can be created in SPIFFS
    String dbPath = "/sensor_data.db";  // SQLite database path
    File dbFile = SPIFFS.open(dbPath, FILE_WRITE);
    if (!dbFile) {
        Serial.println("Failed to create database file in SPIFFS");
        return;
    }
    dbFile.close();  // Close file after confirming it's created

    // Open the SQLite database
    int rc = sqlite3_open("/spiffs/sensor_data.db", &db);
    if (rc) {
        Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
        return;
    } else {
        Serial.println("Database opened successfully");
    }

    // SQL query to create a table with separate timestamps for each sensor
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS SensorData ("
        "ID INTEGER PRIMARY KEY, "  
        "Sensor1 REAL, "
        "Sensor1Timestamp TEXT, "
        "Sensor2 REAL, "
        "Sensor2Timestamp TEXT, "
        "Sensor3 REAL, "
        "Sensor3Timestamp TEXT, "
        "Date TEXT);";  // New Date column added

    // Execute the SQL query to create the table
    rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.printf("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        Serial.println("Table created successfully");
    }

    // Print the contents of the table
    printTable();
}


