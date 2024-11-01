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
