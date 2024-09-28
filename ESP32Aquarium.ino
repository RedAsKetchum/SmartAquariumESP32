#include <SPIFFS.h>
#include <sqlite3.h>
#include <WiFi.h>
#include <time.h> 
#include <ESP32Servo.h> // Include the ESP32Servo library
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoJson.h>

//************************* VARIABLES *****************************************/
sqlite3 *db;
char *zErrMsg = 0;
int rc;
unsigned long previousMillis = 0;   // To store the last time you inserted data
const long interval = 10000;         // Interval between data insertions
float lastSensor1Value = 0;  // Global variable to track the last sent Sensor1 value

/************************* WiFi Access Point *********************************/
#define WLAN_SSID       "Battle_Network"
#define WLAN_PASS       "Pandy218!"

/************************* Adafruit IO Config *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // Use 8883 for SSL
#define AIO_USERNAME    "RedAsKetchum"
#define AIO_KEY         "aio_FXeu11JxZcmPv3ey6r4twxbIyrfH"

/************************** Global State Variables ***************************/
Servo myservo;
const int servoPin = 25;  // GPIO for Servo
int currentPosition = 0;  // To track current servo position

// Create an ESP32 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup a feed for controlling the servo motor
Adafruit_MQTT_Subscribe servoControlFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/servo-control");

// Define the feed where you will publish sensor data
Adafruit_MQTT_Publish sensorDataFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature-sensor");


/************************** Functions ***********************************/
// Function to connect to Wi-Fi
void connectWiFi() {
  Serial.print("Connecting to WiFi...\n");
  Serial.print("");
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  Serial.println("Connected to the WiFi Network!");
}

// Function to connect to Adafruit IO (MQTT)
void connectMQTT() {
  int8_t ret;

  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to Adafruit IO... ");
  
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }

  Serial.println("Connected to Adafruit IO!");
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



//***************************************** MAIN LOGIC OF THE PROGRAM ***************************************/
void setup() {

  Serial.begin(115200);

  delay(1000);// Gives time for the serial monitor to catch up with the statements that follow

  Serial.println("Starting setup...");

  // Connect to Wi-Fi
  connectWiFi();

  // Set up Time
  setupTime();

  // Seed the random number generator with an analog input (for testing database entries)
  randomSeed(analogRead(0));
  
  //Print ESP32s IP Address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
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
  //printTable();
  
  //Button Setups





}
//******************************** Loop ******************************/ 
void loop() {

  unsigned long currentMillis = millis();

  // Ensure MQTT connection
  connectMQTT();

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

      //Convert String to const char* using c_str()
      const char* jsonString = jsonResponse.c_str();

      // Publish the JSON string to the Adafruit IO feed
      if (!sensorDataFeed.publish(jsonString)) {
        Serial.println("Failed to send sensor data");
      } else {
        Serial.println("Sensor data sent successfully");
      }

      //Serial.println(jsonResponse);

     // Print the table after the insertion
      //printTable();
    }
  }
}
