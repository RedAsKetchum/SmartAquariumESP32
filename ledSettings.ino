#include <Adafruit_NeoPixel.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <WiFi.h>
#include <EEPROM.h>  

// ******************** WiFi credentials *******************************
#define WIFI_SSID       "In Your Area 2G"
#define WIFI_PASSWORD   "lightfield289"

// ******************** Adafruit IO credentials ************************
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883  // Use 8883 for SSL
#define AIO_USERNAME    "RedAsKetchum"  // Your Adafruit IO username
#define AIO_KEY         "aio_FXeu11JxZcmPv3ey6r4twxbIyrfH"  // Your Adafruit IO key


// ******************** NeoPixel strip settings ************************
#define LED_PIN         18   // Pin where the data line is connected (Din)
#define NUM_LEDS        150  // Number of LEDs in the strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ******************** EEPROM settings *******************************
#define EEPROM_SIZE     4    // 3 bytes for RGB, 1 byte for brightness

// ******************** MQTT client setup ******************************
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// ******************** MQTT feeds *************************************
Adafruit_MQTT_Subscribe colorFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led-control");

// ******************** Global variables *******************************
float globalBrightness = 1.0;  // Brightness level (1.0 is full brightness)
bool ledOn = true;  // Start with the LEDs turned on
int lastR = 0, lastG = 255, lastB = 0;  // Store last color, default to green

// ******************** Function to connect to WiFi ********************
void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");
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
}

// ******************** Function to save color and brightness **********
void saveLastColor(int r, int g, int b, float brightness) {
  EEPROM.write(0, r);
  EEPROM.write(1, g);
  EEPROM.write(2, b);
  EEPROM.write(3, (int)(brightness * 255));  // Save brightness as an integer (0-255)
  EEPROM.commit();
}

// ******************** Function to retrieve last color and brightness from EEPROM ***
void retrieveLastColor() {
  lastR = EEPROM.read(0);
  lastG = EEPROM.read(1);
  lastB = EEPROM.read(2);
  globalBrightness = EEPROM.read(3) / 255.0;  // Restore brightness as a float (0-1)
}

// ******************** Setup function *********************************
void setup() {
  Serial.begin(115200);

  // Initialize EEPROM to store RGB and brightness values
  EEPROM.begin(EEPROM_SIZE);

  // Connect to WiFi
  connectWiFi();

  // Initialize the LED strip
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'

  // Retrieve last saved color from EEPROM
  retrieveLastColor();

  // Set initial color
  setLEDColor(lastR, lastG, lastB);

  // Subscribe to the Adafruit IO feed
  mqtt.subscribe(&colorFeed);
}

// ******************** Loop function ***********************************
void loop() {
  // Ensure we are connected to Adafruit IO
  MQTT_connect();

  // Process any incoming messages
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
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
  }
}

// ******************** Function to handle color data *******************
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

// ******************** Function to set the LED color *******************
void setLEDColor(int r, int g, int b) {
  if (ledOn) {  // Only change the color if the LEDs are on
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(r * globalBrightness, g * globalBrightness, b * globalBrightness));
    }
    strip.show();  // Send the color data to the strip
  }
}

// ******************** Function to handle on/off commands **************
void handleOnOff(const char* data) {
  if (strcmp(data, "ON") == 0) {
    ledOn = true;
    Serial.println("Turning LEDs on");

    // When turning on, restore the last color
    setLEDColor(lastR, lastG, lastB);
  } else if (strcmp(data, "OFF") == 0) {
    ledOn = false;
    Serial.println("Turning LEDs off");

    strip.clear();  // Turn off the strip (clear all LEDs)
    strip.show();
  }
}

// ******************** Function to adjust brightness *******************
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

