void loop() {
  // Keep the connection to Adafruit IO alive
  io.run();

  // Ensure we are connected to Adafruit IO using MQTT
  MQTT_connect();

  // Process any incoming messages from Adafruit IO
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
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
  }

  // Reset executed flags if a new day has started
  resetExecutedFlagsIfNewDay();

  // Check and control LEDs based on the schedule
  if (scheduleCount > 0) {
    checkScheduleAndControlLED();
  }

  // Periodically fetch schedules to update IDs every 60 seconds
  static unsigned long lastFetch = 0;
  unsigned long now = millis();
  if (now - lastFetch > 60000) {
    fetchSchedulesFromAdafruitIO();
    lastFetch = now;
  }

  // Check if Wi-Fi is connected, and reconnect if necessary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected! Attempting to reconnect...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  delay(2000);  // Short delay to avoid flooding serial output
}
