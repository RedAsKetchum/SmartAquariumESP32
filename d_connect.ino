void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());  // Print the IP address
}

// ******************** Function to connect to Adafruit IO *************
void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to Adafruit IO...");
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(5000);  // Wait 5 seconds before retrying
  }
  Serial.println("Connected to Adafruit IO!");

  // Subscribe to feeds after connection
  if (mqtt.subscribe(&scheduleFeed)) {
    Serial.println("Subscribed to Schedule Feed.");
  } else {
    Serial.println("Failed to subscribe to Schedule Feed.");
  }
}
