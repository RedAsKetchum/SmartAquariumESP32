#include <SPIFFS.h>
#include <sqlite3.h>
#include <WiFi.h>
#include <time.h> 
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h> // Include the ESP32Servo library

//*****************VARIABLES**********************
sqlite3 *db;
char *zErrMsg = 0;
int rc;
const char* ssid = "Battle_Network";
const char* password = "Pandy218!";
unsigned long previousMillis = 0;   // To store the last time you inserted data
const long interval = 10000;         // Interval between data insertions
float lastSensor1Value = 0;  // Global variable to track the last sent Sensor1 value

Servo myservo;  // Create a servo object
const int servoPin = 25;

// Create an instance of the server
AsyncWebServer server(80);

// Function to handle the long polling for Sensor1
void handleLongPolling(AsyncWebServerRequest *request) {
  String jsonResponse = fetchNewestEntryAsJson();
  float currentSensor1Value = getCurrentSensor1Value();  // Fetch the latest sensor1 value

  // If the new value is different from the last sent value
  if (currentSensor1Value != lastSensor1Value) {
    lastSensor1Value = currentSensor1Value;  // Update the last sent value

    // Prepare the response to send the newest Sensor1 entry
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonResponse);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  } else {
    // No new data, so store the request to respond later
    // Alternatively, if the request stays open for too long (e.g., 10 seconds), respond with an empty result.
    //request->send(200, "application/json", "{}");  // Respond with empty JSON after timeout
  }
}

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

// Function to format float values to 2 decimal places
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
  Serial.println("ID | Sensor1 | Timestamp  | Sensor2 | Timestamp  | Sensor3 | Timestamp");

  // Fetch each row and print the data
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    float sensor1 = sqlite3_column_double(stmt, 1);
    const unsigned char* sensor1Timestamp = sqlite3_column_text(stmt, 2);
    float sensor2 = sqlite3_column_double(stmt, 3);
    const unsigned char* sensor2Timestamp = sqlite3_column_text(stmt, 4);
    float sensor3 = sqlite3_column_double(stmt, 5);
    const unsigned char* sensor3Timestamp = sqlite3_column_text(stmt, 6);
    
    //Prints Table on serial monitor
    Serial.printf("%d  | %.2f   | %s   | %.2f   | %s   | %.2f   | %s\n", 
                  id, sensor1, sensor1Timestamp, sensor2, sensor2Timestamp, sensor3, sensor3Timestamp);
  }
  
  // Finalize the statement
  sqlite3_finalize(stmt);
  Serial.println("------------------------------");
}

// Function to get the current timestamp as a string
String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buffer[9];  // Adjust buffer size for just the time
  strftime(buffer, sizeof(buffer), "%I:%M %p", timeInfo);  // Format only the time
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

// Function to fetch the newest entry for sensor data and return it as JSON
String fetchNewestEntryAsJson() {
  const char* sqlSelectNewestEntry = 
    "SELECT ID, Sensor1, Sensor1Timestamp, Sensor2, Sensor2Timestamp, Sensor3, Sensor3Timestamp "
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

    // Format the JSON data
    jsonResult += "\"ID\":" + String(id) + ",";
    jsonResult += "\"Sensor1\":" + formatValue(sensor1) + ",";
    jsonResult += "\"Sensor1Timestamp\":\"" + String((char*)sensor1Timestamp) + "\",";
    jsonResult += "\"Sensor2\":" + formatValue(sensor2) + ",";
    jsonResult += "\"Sensor2Timestamp\":\"" + String((char*)sensor2Timestamp) + "\",";
    jsonResult += "\"Sensor3\":" + formatValue(sensor3) + ",";
    jsonResult += "\"Sensor3Timestamp\":\"" + String((char*)sensor3Timestamp) + "\"";
  } else {
    return "{}";  // Return empty JSON if no data is found
  }

  jsonResult += "}";
  
  // Finalize the statement
  sqlite3_finalize(stmt);
  return jsonResult;
}

// Function to handle CORS preflight requests
void handleCORS(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  request->send(response);
}


//Button Functions
//Feeder Action 
void moveServoSlow(int startAngle, int endAngle) {

  // Move the servo in small steps with a delay to control speed
  if (startAngle < endAngle) {
    // Move forward
    for (int pos = startAngle; pos <= endAngle; pos++) {
      myservo.write(pos);  // Move the servo to the current position
      delay(15);           // Wait 30 ms between steps to slow down movement
    }
  } else {
    // Move backward
    for (int pos = startAngle; pos >= endAngle; pos--) {
      myservo.write(pos);  // Move the servo to the current position
      delay(20);           // Wait 15 ms between steps/degree to slow down movement
    }
  }
}



//*******************************************MAIN LOGIC OF THE PROGRAM*****************************************
void setup() {

  Serial.begin(115200);
  Serial.println("Starting setup...");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

   // Seed the random number generator with an analog input
  randomSeed(analogRead(0));
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Handle CORS for preflight requests (OPTIONS method)
  server.on("/getNewestEntry", HTTP_OPTIONS, handleCORS);
  
  // Define the behavior for when the root URL ("/") is accessed
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Client connected to root URL");
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Hello, this is ESP32 Async Web Server!");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  // Define the route to get the newest sensor entry as JSON
  server.on("/getNewestEntry", HTTP_GET, [](AsyncWebServerRequest *request) {
    String jsonResponse = fetchNewestEntryAsJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonResponse);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  // Handle CORS headers for all requests
  server.onNotFound([](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", "Not Found");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  // Add long polling route for Sensor1
  server.on("/long-polling-sensor1", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleLongPolling(request);
  });

  // Start the server
  Serial.println("Server initialized and ready to listen for incoming client connections (JSON)...");
  server.begin();

  // Set up Time
  setupTime();

  // Mount SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS, formatting now...");
    if (SPIFFS.format()) {
      Serial.println("SPIFFS formatted successfully, retrying mount...");
      if (!SPIFFS.begin()) {
        Serial.println("Failed to mount SPIFFS after formatting.");
        return;
      } else {
        Serial.println("SPIFFS mounted successfully after formatting.");
      }
    } else {
      Serial.println("SPIFFS formatting failed.");
      return;
    }
  } else {
    Serial.println("SPIFFS mounted successfully.");
  }

  // Ensure the database file can be created in SPIFFS
  String dbPath = "/sensor_data.db";  // SQLite database path
  File dbFile = SPIFFS.open(dbPath, FILE_WRITE);
  if (!dbFile) {
    Serial.println("Failed to create database file in SPIFFS");
    return;
  }
  dbFile.close();  // Close file after confirming it's created

  // Open the SQLite database
  rc = sqlite3_open("/spiffs/sensor_data.db", &db);
  if (rc) {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(db));
    return;
  } else {
    Serial.println("Database opened successfully");
  }

  // Print available heap memory to check if memory is sufficient
  Serial.printf("Available heap: %d bytes\n", ESP.getFreeHeap());

  // SQL query to create a table with separate timestamps for each sensor
  const char *sql = 
    "CREATE TABLE IF NOT EXISTS SensorData ("
    "ID INTEGER PRIMARY KEY, "  // Simplified without AUTOINCREMENT
    "Sensor1 REAL, "
    "Sensor1Timestamp TEXT, "
    "Sensor2 REAL, "
    "Sensor2Timestamp TEXT, "
    "Sensor3 REAL, "
    "Sensor3Timestamp TEXT);";
  
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
  
  //Button Setups
  // Attach the servo to the specified pin
  myservo.attach(servoPin);  // Using default pulse width range
  myservo.write(80);  // Move to the neutral position (80 degrees)

  // Define the web server route for moving the servo
  server.on("/move_servo", HTTP_GET, [](AsyncWebServerRequest *request){
    moveServoSlow(10, 0);  // Move backward 30 degrees slowly
    delay(1000);           // Pause for 1 second
    moveServoSlow(0, 50);  // Move forward 30 degrees slowly
    request->send(200, "text/plain", "Servo moved 30 degrees forward and back");
  });

  // Start the server
  server.begin();
  Serial.println("Server started and ready to accept requests.");
}
  
void loop() {

 unsigned long currentMillis = millis();

  // Check if the interval has passed
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Simulate sensor readings (replace with actual sensor values)
    // Generate a random float between 0 and 100
  float sensor1Value = random(0, 10000) / 100.0;  // Sensor 1 reading
  String sensor1Timestamp = getTimestamp();
  
  float sensor2Value = 19.78;  // Sensor 2 reading
  String sensor2Timestamp = getTimestamp();
  
  float sensor3Value = 30.12;  // Sensor 3 reading
  String sensor3Timestamp = getTimestamp();

  // Format sensor values to 2 decimal places
  String formattedSensor1 = formatValue(sensor1Value);
  String formattedSensor2 = formatValue(sensor2Value);
  String formattedSensor3 = formatValue(sensor3Value);

  // Insert the sensor data into the SQLite database
  String sqlInsert = "INSERT INTO SensorData (Sensor1, Sensor1Timestamp, Sensor2, Sensor2Timestamp, Sensor3, Sensor3Timestamp) VALUES (" +
                     formattedSensor1 + ", '" + sensor1Timestamp + "', " + 
                     formattedSensor2 + ", '" + sensor2Timestamp + "', " + 
                     formattedSensor3 + ", '" + sensor3Timestamp + "');";

  // Execute the SQL insert command
  rc = sqlite3_exec(db, sqlInsert.c_str(), 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.printf("SQL error during data insertion: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);  // Free memory for error message
  } else {
    //Serial.println("Inserted sensor data successfully.");
      // Fetch the newest entry as JSON and print it
      String jsonResponse = fetchNewestEntryAsJson();
      Serial.println(jsonResponse);

     // Print the table after the insertion
      //printTable();
    }
  }
}
