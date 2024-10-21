void saveLastColor(int r, int g, int b, float brightness) {
  EEPROM.write(0, r);
  EEPROM.write(1, g);
  EEPROM.write(2, b);
  EEPROM.write(3, (int)(brightness * 255));  
  EEPROM.commit();
}

void retrieveLastColor() {
  lastR = EEPROM.read(0);
  lastG = EEPROM.read(1);
  lastB = EEPROM.read(2);
  globalBrightness = EEPROM.read(3) / 255.0;  
}

void handleColorData(char* data) {
  int r, g, b;
  float brightness = 1.0;
  sscanf(data, "%d,%d,%d,%f", &r, &g, &b, &brightness);  // Parse RGB and brightness values

  // Store the last received color values and brightness
  lastR = r;
  lastG = g;
  lastB = b;
  globalBrightness = brightness;

  // Save to EEPROM
  saveLastColor(r, g, b, brightness);

  setLEDColor(r, g, b);
}

void setLEDColor(int r, int g, int b) {
    if (ledOn) {
        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, strip.Color(r * globalBrightness, g * globalBrightness, b * globalBrightness));
        }
        strip.show();
    }
}

void handleOnOff(const char* data) {
    if (data) {
        // Handle specific "ON" or "OFF" input
        if (strcmp(data, "ON") == 0) {
            ledOn = true;
            Serial.println("Turning LEDs ON");

            // When turning on, restore the last color
            setLEDColor(lastR, lastG, lastB);
        } else if (strcmp(data, "OFF") == 0) {
            ledOn = false;
            Serial.println("Turning LEDs OFF");

            strip.clear();  // Turn off the strip (clear all LEDs)
            strip.show();
        }
    } else {
        // Toggle LED state if no data is provided
        if (!ledOn) {
            ledOn = true;
            Serial.println("Turning LEDs ON");
            setLEDColor(lastR, lastG, lastB);
        } else {
            ledOn = false;
            Serial.println("Turning LEDs OFF");
            strip.clear();
            strip.show();
        }
    }
}

void adjustBrightness(float brightness) {
  globalBrightness = brightness;
  if (ledOn) {
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      strip.setPixelColor(i, strip.Color(r * brightness, g * brightness, b * brightness));
    }
    strip.show();
  }
}
