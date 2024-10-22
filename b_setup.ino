void setup() {
    // Start the serial connection for debugging
    Serial.begin(115200);

    // Initialize EEPROM to store RGB and brightness values
    EEPROM.begin(EEPROM_SIZE);

    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to Wi-Fi...");
    
    // Wait for the connection to complete
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    // When connected, print the IP address
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());  // Display the IP address of the ESP32

    // Configure time zone and NTP server
    configTzTime(timeZone, ntpServer);

    // Initialize the LED strip
    strip.begin();
    strip.show();  // Initialize all pixels to 'off'

    // Retrieve last saved color from EEPROM
    retrieveLastColor();

    // Set initial LED color
    setLEDColor(lastR, lastG, lastB);

    // Connect to Adafruit IO
    io.connect();

    // Set up a message handler for the reboot command feed
    rebootFeed->onMessage(handleRebootCommand);

    // Wait for connection to Adafruit IO
    while (io.status() < AIO_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("\nConnected to Adafruit IO!");

    // Fetch and sort schedules from Adafruit IO
    fetchSchedulesFromAdafruitIO();
    sortSchedules();

    // Subscribe to Adafruit IO feeds
    mqtt.subscribe(&colorFeed);
    mqtt.subscribe(&scheduleFeed);

    Serial.println("LEDs initialized and turned ON.");
   
}
