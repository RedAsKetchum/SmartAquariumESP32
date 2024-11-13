#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "time.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>  
#include <algorithm>  
#include "AdafruitIO_WiFi.h"
#include <SPIFFS.h>
#include <sqlite3.h>
#include <ESP32Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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
#define LED_PIN         15  // Pin where the data line is connected (Din)
#define NUM_LEDS        150   // Number of LEDs in the strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// EEPROM settings
#define EEPROM_SIZE     4     // 3 bytes for RGB, 1 byte for brightness

AdafruitIO_WiFi io(AIO_USERNAME, AIO_KEY, WIFI_SSID, WIFI_PASSWORD);

// MQTT client setup
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// MQTT feeds
Adafruit_MQTT_Subscribe colorFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led-control");
Adafruit_MQTT_Subscribe scheduleFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/feeding-schedule");
AdafruitIO_Feed *rebootFeed = io.feed("reboot-action");
AdafruitIO_Feed *formatFeed = io.feed("format-action");
Adafruit_MQTT_Subscribe servoFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/servo-control");
Adafruit_MQTT_Publish sensorDataFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature-sensor");
Adafruit_MQTT_Subscribe sensorSettingsFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/sensor-settings");
Adafruit_MQTT_Publish notifications = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/notifications"); 
Adafruit_MQTT_Publish wifiNetworkFeed = Adafruit_MQTT_Publish(&mqtt,AIO_USERNAME "/feeds/wifi-network");

struct Schedule {
    String time;
    String days;
    String id;
    bool enabled;
    bool executed;
    String device;
    int scheduledDispenses;
};

// Global variables
float globalBrightness = 1.0;  // Brightness level (1.0 is full brightness)
bool ledOn = true;             // LED starts on
int lastR = 0, lastG = 255, lastB = 0;  // Store last color, default to green
String scheduledTime = "";      // Time set in the schedule
String scheduledDays = "";      // Days set in the schedule
bool scheduleEnabled = false;   // State of the schedule toggle switch
unsigned long lastCheck = 0;  // Track the last time the schedule was checked
const unsigned long checkInterval = 2000;  
Schedule schedules[10];  // Array to store up to 10 schedules
int scheduleCount = 0;   // Keep track of how many schedules are stored
int remainingDispenses[10]; 
bool schedulesNeedFetching = false;
unsigned long lastFetch = 0;
unsigned long fetchInterval = 2000;

//Chris' Variables
sqlite3 *db;
char *zErrMsg = 0;
int rc;
unsigned long previousMillis = 0;   // To store the last time you inserted data
const long interval = 8000;      // Interval between data insertions
float lastSensor1Value = 0;  

//Temperature Sensor
#define ONE_WIRE_BUS 33       // Data wire is connected to pin 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);// Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature tempSensor(&oneWire); // Pass the oneWire reference to DallasTemperature library

//pH Sensor
// Define the pin where the pH sensor is connected
#define PH_SENSOR_PIN 34  // GPIO34 (ADC pin) of ESP32 for analog input

// Calibration values for pH sensor V1 (adjust if necessary)
#define OFFSET 0.00         // pH offset for calibration
#define SAMPLING_INTERVAL 1000  // Interval for pH reading (1 second)  

Servo myServo;
const int servoPin = 13;  // GPIO for Servo
bool servoActive = false;  // Track if the servo is active
unsigned long servoMoveStartTime = 0;  // Time when servo started moving
int servoState = 0;      // Track servo state (0 = idle, 1 = moving to 45 degrees, 2 = returning)

// Variables for pH calculation
float voltage;
float pHValue;
unsigned long lastSampleTime = 0;

// Connect
void connectWiFi();
void MQTT_connect();

// Schedule
void sortSchedules();
void printAllEnabledSchedules();
bool compareSchedules(const Schedule &a, const Schedule &b);
int timeToMinutes(String timeStr);
void fetchSchedulesFromAdafruitIO();
int findScheduleByID(String id);
void updateScheduleInAdafruitIO(String id, bool executed, bool enabled, String time, String days, String device, int scheduledDispenses);
void addSchedule(String time, String days, bool enabled, String id, bool executed, String device, int scheduledDispenses);
void deleteSchedule(int index);
void printAllSchedules();
void retryFetchingSchedules();
void handleScheduleData(char* scheduleData);
int findScheduleByTimeAndDays(String time, String days);
void checkScheduleAndControlDevices();
void resetExecutedFlagsIfNewDay();
bool compareSchedulesForChanges(const Schedule &localSchedule, const Schedule &newSchedule);
void sendLEDStateToAdafruitIO(bool state);
void sendToServoControlFeed(String type, String action, String time, int amount);
void monitorScheduleChanges();
void initializeDatabase();

// LED Settings
void retrieveLastColor();
void saveLastColor(int r, int g, int b, float brightness);
void handleColorData(char* data);
void setLEDColor(int r, int g, int b);
void handleOnOff(const char* data);
void adjustBrightness(float brightness);

// Reboot
void handleRebootCommand(AdafruitIO_Data *data);
// Format
void handleFormatCommand(AdafruitIO_Data *data);

//Chris'
float getCurrentSensor1Value();
String formatValue(float value);
String getTimestamp();
String getFormattedDate();
void setupTime();
String fetchNewestEntryAsJson();
void activateServo();
void handleServoMovement();
