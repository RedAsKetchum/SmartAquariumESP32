void setup() {
    // Start the serial connection for debugging
    Serial.begin(115200);
    delay(1000); // Gives time for the serial monitor to catch up with the statements that follow

    Serial.println("Starting setup...");

    // Connect to Wi-Fi
    connectWiFi();  // Assuming you have a custom function for connecting to Wi-Fi

    // Set up Time
    setupTime();  // Assuming you have a custom function for setting up time
    configTzTime(timeZone, ntpServer);
    
    // Seed the random number generator with an analog input (for testing database entries)
    randomSeed(analogRead(0));

    // Configure the pin mode for pH sensor as an input
    pinMode(PH_SENSOR_PIN, INPUT);

    // Start up the DS18B20 library for temperature sensor
    tempSensor.begin();

    // ************** Insert Turbidity Code Here *************

    // Manual format in case of corruption
    //SPIFFS.format();

    // Mount SPIFFS for file storage
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

    // ********** Buttons: Setup Adafruit IO subscription ***********/
    // Servo Motor
    mqtt.subscribe(&servoFeed);
    myServo.attach(servoPin);
    myServo.write(57);  // Initialize the servo to the original position (57 degrees)

    // Initialize EEPROM to store RGB and brightness values
    EEPROM.begin(EEPROM_SIZE);

    // Retrieve last saved color from EEPROM
    retrieveLastColor();

    // Set initial LED color
    setLEDColor(lastR, lastG, lastB);

    // Initialize the LED strip
    strip.begin();
    strip.show();  // Initialize all pixels to 'off'

    // Connect to Adafruit IO
    io.connect();

    // Set up a message handler for the reboot command feed
    rebootFeed->onMessage(handleRebootCommand);

    // Wait for connection to Adafruit IO
    while (io.status() < AIO_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println(F("\nConnected to Adafruit IO!"));

    // Fetch and sort schedules from Adafruit IO
    fetchSchedulesFromAdafruitIO();
    sortSchedules();

    // Subscribe to Adafruit IO feeds
    mqtt.subscribe(&colorFeed);
    mqtt.subscribe(&scheduleFeed);

    Serial.println(F("LEDs initialized and turned ON."));
    
    // Print available heap memory
    Serial.printf("Available heap: %d bytes\n", ESP.getFreeHeap());
}
