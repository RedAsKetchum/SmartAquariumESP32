#include "AdafruitIO_WiFi.h"
#include <WiFi.h>  // Include Wi-Fi library for IP address handling

// Set up Adafruit IO credentials
#define IO_USERNAME "RedAsKetchum"
#define IO_KEY "aio_FXeu11JxZcmPv3ey6r4twxbIyrfH"


// Wi-Fi credentials
#define WIFI_SSID "In Your Area-2G"
#define WIFI_PASS "lightfield289"

// Create an instance of the Adafruit IO WiFi class
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Set up the feed for the reboot command
AdafruitIO_Feed *rebootFeed = io.feed("reboot-action");

void setup() {
  // Start the serial connection for debugging
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
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
  Serial.println("Subscribed to reboot-action feed");
}

void loop() {
  // Keep the connection to Adafruit IO alive
  io.run();
  Serial.println("Listening for commands...");
  delay(2000);

  // Check if Wi-Fi is connected, and reconnect if necessary
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected! Attempting to reconnect...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// Function to handle reboot commands
void handleRebootCommand(AdafruitIO_Data *data) {
  Serial.println("Message received from Adafruit IO:");
  
  if (data) {
    String command = data->toString();  // Convert the received data to a string
    Serial.println("Command: " + command);  // Log received command

    // Check if the command is "reboot"
    if (command == "reboot") {
      Serial.println("Reboot command received. Rebooting...");
      ESP.restart();  // Perform the reboot
    } else {
      Serial.println("Unknown command received: " + command);
    }
  } else {
    Serial.println("No data received from Adafruit IO.");
  }
}
