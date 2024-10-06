#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>

// ******************** Adafruit IO credentials ************************
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883  // Use 8883 for SSL
#define AIO_USERNAME    "RedAsKetchum"  // Your Adafruit IO username
#define AIO_KEY         "aio_FXeu11JxZcmPv3ey6r4twxbIyrfH"  // Your Adafruit IO key

// ******************** WiFi credentials *******************************
#define WIFI_SSID       "In Your Area 2G"
#define WIFI_PASSWORD   "lightfield289"

// ******************** NeoPixel strip settings ************************
#define LED_PIN         18   // Pin where the data line is connected (Din)
#define NUM_LEDS        150  // Number of LEDs in the strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

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

// ******************** Setup function *********************************
void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  connectWiFi();

  // Initialize the LED strip
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'

  // Set initial color to green
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
        // If it's not "ON" or "OFF", treat it as RGB values
        handleColorData(controlData);
      }
    }
  }
}

// ******************** Function to handle color data *******************
void handleColorData(char* data) {
  int r, g, b;
  sscanf(data, "%d,%d,%d", &r, &g, &b);  // Parse the RGB values

  // Store the last received color values
  lastR = r;
  lastG = g;
  lastB = b;

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

