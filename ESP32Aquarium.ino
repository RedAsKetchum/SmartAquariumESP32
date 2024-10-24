#include <SPIFFS.h>
#include <sqlite3.h>
#include <WiFi.h>
#include <time.h> 
#include <ESP32Servo.h> //servo motor
#include <Arduino.h> //pH sensor
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>// temperature sensor

//NOTE: UPDATE CODE TO SEND SENSOR DATA EVERY 10 SECONDS BUT ONLY INSERT ON TABLE ONCE PER HOUR FOR 24 HOURS!!!

//********** VARIABLES ***********/
sqlite3 *db;
char *zErrMsg = 0;
int rc;
unsigned long previousMillis = 0;   // To store the last time you inserted data
const long interval = 10000;      // Interval between data insertions
float lastSensor1Value = 0;    

//Temperature Sensor
#define ONE_WIRE_BUS 33       // Data wire is connected to pin 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);// Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature tempSensor(&oneWire); // Pass the oneWire reference to DallasTemperature library

//pH Sensor
// Define the pin where the pH sensor is connected
#define PH_SENSOR_PIN 34  // GPIO34 (ADC pin) of ESP32 for analog input

// Calibration values for pH sensor V1 (adjust if necessary)
#define OFFSET 0.00         // pH offset for calibration
#define SAMPLING_INTERVAL 1000  // Interval for pH reading (1 second)

/************************* WiFi Access Point *********************************/
#define WLAN_SSID       "Battle_Network"
#define WLAN_PASS       "Pandy218!"

/************************* Adafruit IO Config *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // Use 8883 for SSL
#define AIO_USERNAME    "RedAsKetchum"
#define AIO_KEY         "aio_FXeu11JxZcmPv3ey6r4twxbIyrfH"

/************************** Global State Variables ***************************/
Servo myServo;
const int servoPin = 25;  // GPIO for Servo
bool servoActive = false;  // Track if the servo is active
unsigned long servoMoveStartTime = 0;  // Time when servo started moving
int servoState = 0;      // Track servo state (0 = idle, 1 = moving to 45 degrees, 2 = returning)

// Variables for pH calculation
float voltage;
float pHValue;
unsigned long lastSampleTime = 0;

// Create an ESP32 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup a feed for controlling the servo motor
Adafruit_MQTT_Subscribe servoFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/servo-control");

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

// Function to get the current timestamp as a string
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

// Function to fetch the newest entry for sensor data and return it as JSON
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

//******************************* Buttons ********************************/:
void activateServo() {
  if (!servoActive) {
    // Move servo to 0 degrees (towards dispensing opening)
    myServo.write(0);
    servoActive = true;
    servoMoveStartTime = millis();  // Record when the movement started
    servoState = 1;  // Set state to "moving to 45 degrees"
  }
}

// Function to handle the servo logic
void handleServoMovement() {
  unsigned long currentMillis = millis();

  // Check the servo state
  if (servoState == 1 && currentMillis - servoMoveStartTime >= 500) {
    //  Half a second have passed, return the servo to the original position (57 degrees)
    myServo.write(57);
    servoMoveStartTime = currentMillis;  // Reset the timer for returning to 0 degrees 
    servoState = 2;  // Set state to "returning"
  } //else if (servoState == 2 && currentMillis - servoMoveStartTime >= 6000) {
    else if (servoState == 2) {
    // Servo has returned, reset to idle state
    servoState = 0;
    servoActive = false;
  }
}

//***************************** MAIN LOGIC OF THE PROGRAM **********************************/
void setup() {

  Serial.begin(115200);

  delay(1000);// Gives time for the serial monitor to catch up with the statements that follow

  Serial.println("Starting setup...");

  // Connect to Wi-Fi
  connectWiFi();

  // Set up Time
  setupTime();
  
  //Print ESP32s IP Address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

    // Seed the random number generator with an analog input (for testing database entries)
  randomSeed(analogRead(0));

   // Configure the pin mode for pH sensor as an input
  pinMode(PH_SENSOR_PIN, INPUT);
  //Serial.println("pH Sensor (SEN0161 V1) ready...");

  // Start up the DS18B20 library
  tempSensor.begin();

  //************** Insert Turbidity Code Here *************
  
  //Manual format in case of corruption
  //SPIFFS.format();

  // Mount SPIFFS
  if (!SPIFFS.begin(true)) {
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
  
  // ********** Buttons: Setup Adafruit IO subscription ***********/
  //Servo Motor
  mqtt.subscribe(&servoFeed);
  // Attach the servo to the pin
  myServo.attach(servoPin);
  // Initialize the servo to the original position (57 degrees)
  myServo.write(57);

}
//******************************** Loop ******************************/ 
void loop() {
  unsigned long currentMillis = millis();

  // Ensure MQTT connection
  connectMQTT();

 // Check for new data from the Adafruit IO feed
  Adafruit_MQTT_Subscribe *subscription;

  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &servoFeed) {
    
      String data = (char *)servoFeed.lastread;

      // Trigger the servo when "activate" message is received
      if (data == "activate") {
        activateServo();  // Call the function to activate the servo
      }
    }
  }

  // Check if the interval has passed before inserting a new entry on the table
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

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
    
    //Serial.printf("Number of entries in the database: %d\n", rowCount);

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
        Serial.println("Oldest entry deleted successfully.");
      }
    }
     
    //Read Temperature sensor value 
    tempSensor.requestTemperatures();

    // Fetch temperature in Celsius from the sensor and convert to Farenheit
    // Index 0 assumes one sensor is connected
    float temperatureC = tempSensor.getTempCByIndex(0);
    float temperatureF = tempSensor.toFahrenheit(temperatureC);
    String sensor1Timestamp = getTimestamp();

    // Read the analog value from the pH sensor (10-bit ADC: 0-4095)
    int analogValue = analogRead(PH_SENSOR_PIN);

    // Convert the analog value (0-4095) to a voltage (0-3.3V)
    voltage = analogValue * (3.3 / 4095.0);

    // Calculate pH using the formula for pH V1 sensor (usually 3.5 * voltage)
    pHValue = 3.5 * voltage + OFFSET;
    String sensor2Timestamp = getTimestamp();
    
    float sensor3Value = random(0, 10000) / 100.0;  // Sensor 3 random reading
    String sensor3Timestamp = getTimestamp();

    // Format sensor values to 2 decimal places
    String formattedSensor1 = formatValue(temperatureF);
    String formattedSensor2 = formatValue(pHValue);
    String formattedSensor3 = formatValue(sensor3Value);

    // Get the current date formatted as MM.DD
    String formattedDate = getFormattedDate();
    
    // Insert the sensor data and the formatted date into the SQLite database
   String sqlInsert = "INSERT INTO SensorData (Sensor1, Sensor1Timestamp, Sensor2, Sensor2Timestamp, Sensor3, Sensor3Timestamp, Date) VALUES (" +
                   formattedSensor1 + ", '" + sensor1Timestamp + "', " + 
                   formattedSensor2 + ", '" + sensor2Timestamp + "', " + 
                   formattedSensor3 + ", '" + sensor3Timestamp + "', '" + 
                   formattedDate + "');";

    // Execute the SQL insert command
    rc = sqlite3_exec(db, sqlInsert.c_str(), 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
      Serial.printf("SQL error during data insertion: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);  // Free memory for error message
    } else {
      //Serial.println("Inserted new sensor data successfully.");
    }

    // Fetch the newest entry as JSON and print it
    String jsonResponse = fetchNewestEntryAsJson();

    // Convert String to const char* using c_str()
    const char* jsonString = jsonResponse.c_str();

    // Publish the JSON string to the Adafruit IO feed
    if (!sensorDataFeed.publish(jsonString)) {
      Serial.println("Failed to send sensor data to Adafruit IO.");
    } else {
      Serial.println("Sensor data sent to Adafruit IO...");
    }

    Serial.println(jsonResponse);

    // Print the table after the insertion or update
    printTable();
  }

   // Handle servo movement asynchronously
   handleServoMovement();  // Ensure the servo logic runs in the background
}
