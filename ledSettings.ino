#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>

// WiFi credentials
#define WIFI_SSID       "In Your Area 2G"
#define WIFI_PASSWORD   "lightfield289"

// Adafruit IO credentials
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                  // Use 8883 for SSL
#define AIO_USERNAME    "jazzfaye7"           // Your Adafruit IO username
#define AIO_KEY         "aio_XHzM43BcDX46OMZ5ZoG0DYoN6zDr"  // Your Adafruit IO key

// Define LED strip parameters
#define LED_PIN         18   // Pin where the data line is connected (Din)
#define NUM_LEDS        150   // Number of LEDs in the strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Initialize WiFi and MQTT client
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Define MQTT topics for color and brightness
Adafruit_MQTT_Subscribe colorFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led-control");
Adafruit_MQTT_Subscribe brightnessFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led-brightness");

// Global brightness value
float globalBrightness = 1.0;

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  connectWiFi();

  // Initialize the LED strip
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'

  // Test setting LED to red, green, blue for 1 second each
  strip.setPixelColor(0, strip.Color(255, 0, 0));  // Red
  strip.show();
  delay(1000);
  strip.setPixelColor(0, strip.Color(0, 255, 0));  // Green
  strip.show();
  delay(1000);
  strip.setPixelColor(0, strip.Color(0, 0, 255));  // Blue
  strip.show();
  delay(1000);

  // Subscribe to the Adafruit IO feeds
  mqtt.subscribe(&colorFeed);
  mqtt.subscribe(&brightnessFeed);
}

void loop() {
  // Ensure we are connected to Adafruit IO
  MQTT_connect();

  // Process any incoming messages
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &colorFeed) {
      char *colorData = (char *)colorFeed.lastread;
      Serial.print("Color data received: ");
      Serial.println(colorData);
      handleColorData(colorData);
    }

    if (subscription == &brightnessFeed) {
      globalBrightness = atof((char *)brightnessFeed.lastread);
      Serial.print("Brightness received: ");
      Serial.println(globalBrightness);
      adjustBrightness(globalBrightness);
    }
  }
}

// Function to handle received color data
void handleColorData(char* data) {
  int r, g, b;
  sscanf(data, "%d,%d,%d", &r, &g, &b);  // Parse the RGB values
  setLEDColor(r, g, b);
}

// Function to set the color of the LED strip
void setLEDColor(int r, int g, int b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r * globalBrightness, g * globalBrightness, b * globalBrightness));
  }
  strip.show();  // Send the color data to the strip
}

// Function to adjust brightness by scaling the current colors
void adjustBrightness(float brightness) {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint32_t color = strip.getPixelColor(i);
    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;
    strip.setPixelColor(i, strip.Color(r * brightness, g * brightness, b * brightness));
  }
  strip.show();
}

// Function to connect to WiFi
void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");
}

// Function to connect to Adafruit IO
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
