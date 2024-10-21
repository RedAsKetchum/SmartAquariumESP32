void setup() {
    Serial.begin(115200);

    // Initialize EEPROM to store RGB and brightness values
    EEPROM.begin(EEPROM_SIZE);

    // Connect to WiFi
    connectWiFi();

    // Configure time zone and NTP server
    configTzTime(timeZone, ntpServer);

    // Initialize the LED strip
    strip.begin();
    strip.show();  // Initialize all pixels to 'off'

    // Retrieve last saved color from EEPROM
    retrieveLastColor();

    // Set initial LED color
    setLEDColor(lastR, lastG, lastB);

    // Fetch and sort schedules from Adafruit IO
    fetchSchedulesFromAdafruitIO();
    sortSchedules();

    // Subscribe to Adafruit IO feeds
    mqtt.subscribe(&colorFeed);
    mqtt.subscribe(&scheduleFeed);

    Serial.println("LEDs initialized and turned ON.");
}
