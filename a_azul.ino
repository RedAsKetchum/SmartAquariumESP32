#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "time.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>  // Include the HTTP library
#include <algorithm>  // Include algorithm for sorting

// ******************** WiFi credentials *******************************
#define WIFI_SSID       "In Your Area-2G"
#define WIFI_PASSWORD   "lightfield289"

// NTP server to request time
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "EST5EDT,M3.2.0,M11.1.0";  // Timezone for New York City

// Adafruit IO credentials
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883  // Use 8883 for SSL
#define AIO_USERNAME    "RedAsKetchum"
#define AIO_KEY         "aio_FXeu11JxZcmPv3ey6r4twxbIyrfH"

// NeoPixel strip settings
#define LED_PIN         27   // Pin where the data line is connected (Din)
#define NUM_LEDS        150   // Number of LEDs in the strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// EEPROM settings
#define EEPROM_SIZE     4     // 3 bytes for RGB, 1 byte for brightness

// MQTT client setup
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// MQTT feeds
Adafruit_MQTT_Subscribe colorFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led-control");
Adafruit_MQTT_Subscribe scheduleFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/feeding-schedule");

struct Schedule {
    String time;
    String days;
    String id;
    bool enabled;
    bool executed;  // New flag to track if the schedule has been executed
};

// Global variables
float globalBrightness = 1.0;  // Brightness level (1.0 is full brightness)
bool ledOn = true;             // LED starts on
int lastR = 0, lastG = 255, lastB = 0;  // Store last color, default to green
String scheduledTime = "";      // Time set in the schedule
String scheduledDays = "";      // Days set in the schedule
bool scheduleEnabled = false;   // State of the schedule toggle switch
unsigned long lastCheck = 0;  // Track the last time the schedule was checked
const unsigned long checkInterval = 60000;  // Check every minute
Schedule schedules[10];  // Array to store up to 10 schedules
int scheduleCount = 0;   // Keep track of how many schedules are stored


// Connect
void connectWiFi();
void MQTT_connect();

// LED Schedule
void sortSchedules();
void printAllEnabledSchedules();
bool compareSchedules(const Schedule &a, const Schedule &b);
int timeToMinutes(String timeStr);
void fetchSchedulesFromAdafruitIO();
int findScheduleByID(String id);
void updateScheduleInAdafruitIO(String id, bool executed, bool enabled, String time, String days);
String getDayAbbreviation(const char* fullDay);
void addSchedule(String time, String days, bool enabled, String id, bool executed);
void deleteSchedule(int index);
void printAllSchedules();
void retryFetchingSchedules();
void handleScheduleData(char* scheduleData);
int findScheduleByTimeAndDays(String time, String days);
void checkScheduleAndControlLED();
void resetExecutedFlagsIfNewDay();
bool compareSchedulesForChanges(const Schedule &localSchedule, const Schedule &newSchedule);

// LED Settings
void retrieveLastColor();
void saveLastColor(int r, int g, int b, float brightness);
void handleColorData(char* data);
void setLEDColor(int r, int g, int b);
void handleOnOff(const char* data);
void adjustBrightness(float brightness);