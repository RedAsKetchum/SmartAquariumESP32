void sortSchedules() {
  std::sort(schedules, schedules + scheduleCount, compareSchedules);
  Serial.println("Schedules sorted by time and day.");
}

void printAllEnabledSchedules() {
    Serial.println("Enabled Schedules:");
    for (int i = 0; i < scheduleCount; i++) {
        if (schedules[i].enabled) {
            Serial.printf("Schedule %d: Time: %s, Days: %s, Device: %s, Dispenses: %d, Enabled: %s\n", 
                          i + 1, schedules[i].time.c_str(), schedules[i].days.c_str(), 
                          schedules[i].device.c_str(), schedules[i].scheduledDispenses, 
                          schedules[i].enabled ? "true" : "false");
        }
    }
}

bool compareSchedules(const Schedule &a, const Schedule &b) {
  // Convert time to minutes from midnight
  int timeA = timeToMinutes(a.time);
  int timeB = timeToMinutes(b.time);
  
  // Compare by time first
  if (timeA != timeB) {
    return timeA < timeB;  // Earlier times come first
  }

  // If times are equal, schedules without days (one-time) come first
  if (a.days.isEmpty() && !b.days.isEmpty()) {
    return true;  // a comes first
  } else if (!a.days.isEmpty() && b.days.isEmpty()) {
    return false;  // b comes first
  }

  return false;  // Otherwise, they are equal in sorting priority
}

int timeToMinutes(String timeStr) {
  int hour, minute;
  char ampm[3];
  sscanf(timeStr.c_str(), "%d:%d %s", &hour, &minute, ampm);

  // Convert hour to 24-hour format
  if (strcmp(ampm, "PM") == 0 && hour != 12) {
    hour += 12;
  }
  if (strcmp(ampm, "AM") == 0 && hour == 12) {
    hour = 0;
  }

  return hour * 60 + minute;  // Total minutes from midnight
}

void fetchSchedulesFromAdafruitIO() {
    HTTPClient http;
    String url = String("https://io.adafruit.com/api/v2/") + AIO_USERNAME + "/feeds/feeding-schedule/data";
  
    http.begin(url);  // Initialize HTTP request to the given URL
    http.addHeader("X-AIO-Key", AIO_KEY);  // Add the Adafruit IO Key in the request header

    int httpCode = http.GET();  // Make the GET request

    if (httpCode > 0) {
        String payload = http.getString();  // Get the response as a string
        Serial.println("Fetched schedules from Adafruit IO:");
        Serial.println(payload);  // Print the raw payload for debugging

        // Parse the JSON response and store the schedules
        StaticJsonDocument<2048> doc;  // Increased document size for larger payload
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            JsonArray data = doc.as<JsonArray>();
            bool foundInIO = false;  // Flag to check if schedule exists in Adafruit IO

            // Iterate through each schedule in the response from Adafruit IO
            for (JsonObject schedule : data) {
                String scheduleValue = schedule["value"];  // Get the schedule value (contains JSON)
                String id = schedule["id"];  // Extract the schedule ID from Adafruit IO

                // Parse the value JSON
                StaticJsonDocument<256> valueDoc;
                DeserializationError valueError = deserializeJson(valueDoc, scheduleValue);

                if (!valueError) {
                    String time = valueDoc["time"];
                    String days = valueDoc["days"];
                    bool enabled = valueDoc["enabled"];
                    String device = valueDoc["device"];
                    int scheduledDispenses = valueDoc["scheduledDispenses"];

                    // Find the schedule by ID in the local list
                    int existingIndex = findScheduleByID(id);

                    if (existingIndex != -1) {
                        // If found locally, mark that it exists in Adafruit IO
                        foundInIO = true;

                        // If the schedule is disabled, delete it locally
                        if (!enabled) {
                            Serial.printf("Schedule disabled in Adafruit IO. Deleting locally: ID: %s\n", id.c_str());
                            deleteSchedule(existingIndex);
                        }
                    } else if (enabled) {
                        // Add the schedule if it doesn't exist locally and is enabled
                        addSchedule(time, days, enabled, id, false, device, scheduledDispenses);
                    }
                } else {
                    Serial.println("Error parsing individual schedule JSON.");
                }
            }

            // After checking all schedules from Adafruit IO, remove any local schedules that were not found
            for (int i = 0; i < scheduleCount; i++) {
                foundInIO = false;
                // Re-check if the local schedule exists in Adafruit IO
                for (JsonObject schedule : data) {
                    String id = schedule["id"];
                    if (schedules[i].id == id) {
                        foundInIO = true;
                        break;
                    }
                }

                if (!foundInIO) {
                    // If not found, delete the schedule locally
                    Serial.printf("Schedule not found in Adafruit IO. Deleting locally: ID: %s\n", schedules[i].id.c_str());
                    deleteSchedule(i);
                    i--;  // Adjust index after deletion to prevent skipping schedules
                }
            }
        } else {
            Serial.println("Error parsing JSON from Adafruit IO.");
        }
    } else {
        Serial.printf("Error fetching schedules from Adafruit IO. HTTP code: %d\n", httpCode);
    }

    http.end();  // End the HTTP connection
}

int findScheduleByID(String id) {
    for (int i = 0; i < scheduleCount; i++) {
        if (schedules[i].id == id) {
            return i;  // Return the index if the schedule is found by ID
        }
    }
    return -1;  // Return -1 if no schedule is found with this ID
}

void updateScheduleInAdafruitIO(String id, bool executed, bool enabled, String time, String days, String device, int scheduledDispenses) {
    // If there are no days set (one-time schedule), disable the schedule after execution
    if (days.isEmpty()) {
        enabled = false;  // Disable one-time schedules after execution
    } else {
        enabled = true;   // Keep enabled true for recurring schedules
    }

    // Create a JSON document for the updated schedule data
    StaticJsonDocument<256> scheduleDoc;
    scheduleDoc["time"] = time;
    scheduleDoc["days"] = days;
    scheduleDoc["device"] = device; 
    scheduleDoc["scheduledDispenses"] = scheduledDispenses; 
    scheduleDoc["enabled"] = enabled;
    scheduleDoc["executed"] = executed;

    // Convert the schedule document to a string
    String scheduleOutput;
    serializeJson(scheduleDoc, scheduleOutput);

    // Wrap the schedule JSON string in the "value" field
    StaticJsonDocument<256> payloadDoc;
    payloadDoc["value"] = scheduleOutput;

    // Convert the payload document to a string for the PUT request
    String finalPayload;
    serializeJson(payloadDoc, finalPayload);

    // Send the PUT request to Adafruit IO
    HTTPClient http;
    String url = String("https://io.adafruit.com/api/v2/") + AIO_USERNAME + "/feeds/feeding-schedule/data/" + id;
    http.begin(url);
    http.addHeader("X-AIO-Key", AIO_KEY);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(finalPayload);  // Send the updated schedule wrapped in the "value" field
    if (httpCode > 0) {
        String response = http.getString();
        Serial.println("Response from Adafruit IO after update:");
        Serial.println(response);
    } else {
        Serial.printf("Error updating schedule in Adafruit IO. HTTP code: %d\n", httpCode);
    }

    http.end();  // End the HTTP connection
}

// Function to convert full day name to abbreviated form
String getDayAbbreviation(const char* fullDay) {
    if (strcmp(fullDay, "Sunday") == 0) return "Su";
    if (strcmp(fullDay, "Monday") == 0) return "Mo";
    if (strcmp(fullDay, "Tuesday") == 0) return "Tu";
    if (strcmp(fullDay, "Wednesday") == 0) return "We";
    if (strcmp(fullDay, "Thursday") == 0) return "Th";
    if (strcmp(fullDay, "Friday") == 0) return "Fr";
    if (strcmp(fullDay, "Saturday") == 0) return "Sa";
    return "";  // Return empty string if no match is found
}

void addSchedule(String time, String days, bool enabled, String id, bool executed, String device, int scheduledDispenses) {
    int existingIndex = findScheduleByID(id);

    if (existingIndex != -1) {
        Serial.printf("Schedule already exists: Time: %s, ID: %s\n", time.c_str(), id.c_str());
        return;
    }

    if (scheduleCount < 10) {
        schedules[scheduleCount].time = time;
        schedules[scheduleCount].days = days;
        schedules[scheduleCount].enabled = enabled;
        schedules[scheduleCount].executed = executed;
        schedules[scheduleCount].id = id;
        schedules[scheduleCount].device = device;
        schedules[scheduleCount].scheduledDispenses = scheduledDispenses;
        scheduleCount++;
        Serial.printf("Schedule %d added: Time: %s, Days: %s, Device: %s, Dispenses: %d\n", 
                      scheduleCount, time.c_str(), days.c_str(), device.c_str(), scheduledDispenses);
    } else {
        Serial.println("Schedule limit reached (10 schedules max).");
    }
}

void deleteSchedule(int index) {
    if (index < 0 || index >= scheduleCount) {
        Serial.println("Invalid schedule index for deletion.");
        return;
    }

    Serial.printf("Deleting Schedule %d: Time: %s, Days: %s, ID: %s\n", 
                  index + 1, 
                  schedules[index].time.c_str(), 
                  schedules[index].days.isEmpty() ? "One-time" : schedules[index].days.c_str(),
                  schedules[index].id.c_str());

    // Shift all schedules up to fill the gap left by the deleted schedule
    for (int i = index; i < scheduleCount - 1; i++) {
        schedules[i] = schedules[i + 1];
    }

    scheduleCount--;  // Decrease the total number of schedules
    Serial.printf("Schedule deleted. Remaining schedules: %d\n", scheduleCount);

    // Display the remaining schedules
    if (scheduleCount > 0) {
        Serial.println("Remaining Schedules:");
        for (int i = 0; i < scheduleCount; i++) {
            Serial.printf("Schedule %d: Time: %s, Days: %s, ID: %s\n", 
                          i + 1, 
                          schedules[i].time.c_str(), 
                          schedules[i].days.isEmpty() ? "One-time" : schedules[i].days.c_str(),
                          schedules[i].id.c_str());
        }
    } else {
        Serial.println("No remaining schedules.");
    }
}

void printAllSchedules() {
    Serial.println("All Non-Executed Schedules:");
    for (int i = 0; i < scheduleCount; i++) {
        if (!schedules[i].executed) {
            Serial.printf("Schedule %d: Time: %s, Days: %s, Enabled: %s, Device: %s, Dispenses: %d, ID: %s\n", 
                          i + 1, schedules[i].time.c_str(), 
                          schedules[i].days.isEmpty() ? "One-time" : schedules[i].days.c_str(), 
                          schedules[i].enabled ? "true" : "false",
                          schedules[i].device.c_str(),
                          schedules[i].scheduledDispenses,
                          schedules[i].id.c_str());
        }
    }
}


void retryFetchingSchedules() {
    // Add delay or retry logic to re-fetch schedules
    for (int i = 0; i < 5; i++) {  // Retry up to 5 times
        fetchSchedulesFromAdafruitIO();  // Fetch schedules again
        for (int j = 0; j < scheduleCount; j++) {
            if (schedules[j].id == "unknown") {
                delay(5000);  // Wait for 5 seconds before retrying
            } else {
                return;  // Exit if we got the ID
            }
        }
    }
}

void handleScheduleData(char* scheduleData) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, scheduleData);

    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.f_str());
        return;
    }

    const char* time = doc["time"];
    const char* days = doc["days"];
    bool enabled = doc.containsKey("enabled") ? doc["enabled"] : true;
    String status = doc.containsKey("status") ? String((const char*)doc["status"]) : "pending";
    String id = doc["id"] | "unknown";  // Grab the ID or set to "unknown" if not present
    String device = doc.containsKey("device") ? String((const char*)doc["device"]) : "defaultDevice";
    int scheduledDispenses = doc.containsKey("scheduledDispenses") ? doc["scheduledDispenses"].as<int>() : 1;

    int existingIndex = findScheduleByTimeAndDays(String(time), String(days));

    if (enabled && status != "executed") {
        if (existingIndex == -1) {
            // Add the schedule if it doesn't already exist
            addSchedule(time, days, enabled, id, false, device, scheduledDispenses);
            Serial.printf("Schedule added: Time: %s, Days: %s, Enabled: true, ID: %s\n", time, days, id.c_str());
        } else {
            Serial.printf("Schedule already exists: Time: %s, Days: %s\n", time, days);
        }
    } 
    else if (!enabled && existingIndex != -1) {
        // If the schedule is disabled and exists, delete it
        deleteSchedule(existingIndex);
        Serial.printf("Schedule disabled and deleted: Time: %s, Days: %s\n", time, days);
    } 
    else if (status == "executed") {
        // If the schedule is marked as executed, skip it
        Serial.printf("Schedule already executed, skipping: Time: %s, Days: %s\n", time, days);
    } 
    else {
        Serial.println("Invalid schedule data received.");
    }

    retryFetchingSchedules();
    printAllEnabledSchedules();
}

int findScheduleByTimeAndDays(String time, String days) {
    for (int i = 0; i < scheduleCount; i++) {
        if (schedules[i].time == time && schedules[i].days == days) {
            return i;
        }
    }
    return -1;
}

void checkScheduleAndControlDevices() {
    if (millis() - lastCheck >= checkInterval) {
        lastCheck = millis();

        time_t now = time(nullptr);
        struct tm *timeinfo = localtime(&now);

        int hour = timeinfo->tm_hour;
        String ampm = "AM";
        if (hour >= 12) {
            ampm = "PM";
            if (hour > 12) hour -= 12;
        } else if (hour == 0) {
            hour = 12;
        }

        char currentTime[9];
        sprintf(currentTime, "%02d:%02d %s", hour, timeinfo->tm_min, ampm.c_str());

        for (int i = 0; i < scheduleCount; i++) {
            if (schedules[i].enabled && !schedules[i].executed) {
                Serial.printf("Checking Schedule %d: Time: %s, Days: %s, Device: %s\n", 
                              i + 1, schedules[i].time.c_str(), schedules[i].days.c_str(), schedules[i].device.c_str());

                // Execute only once during the minute by marking it as executed right away
                if (strcmp(currentTime, schedules[i].time.c_str()) == 0) {
                    Serial.printf("Time match! Executing Schedule %d for Device: %s.\n", i + 1, schedules[i].device.c_str());

                    if (schedules[i].device == "LED") {
                        // Turn LED on only once at the scheduled time
                        if (!ledOn) {
                            handleOnOff("ON");
                        } else {
                          handleOnOff("OFF");
                        }
                        schedules[i].executed = true;
                    } 
                    else if (schedules[i].device == "Feeder") {
                        // Initialize remainingDispenses if it hasn't been set
                        if (remainingDispenses[i] == 0) {
                            remainingDispenses[i] = schedules[i].scheduledDispenses;
                        }

                        // Activate servo for each dispense and count down
                        if (remainingDispenses[i] > 0) {
                            activateServo();
                            delay(500);  // Delay between dispenses
                            remainingDispenses[i]--;

                            // Only set executed to true when all dispenses are completed
                            if (remainingDispenses[i] == 0) {
                                schedules[i].executed = true;
                            }
                        }
                    }

                    // Handle one-time schedules or update Adafruit IO as needed
                    if (schedules[i].days.isEmpty() && schedules[i].executed) {
                        schedules[i].enabled = false;
                        updateScheduleInAdafruitIO(schedules[i].id, true, false, schedules[i].time, schedules[i].days, schedules[i].device, schedules[i].scheduledDispenses);
                        deleteSchedule(i);  // Delete one-time schedule locally after execution
                        i--;  // Adjust index after deletion to prevent skipping
                    } else if (schedules[i].executed) {
                        // Update Adafruit IO for recurring schedules
                        updateScheduleInAdafruitIO(schedules[i].id, true, true, schedules[i].time, schedules[i].days, schedules[i].device, schedules[i].scheduledDispenses);
                    }
                }
            }
        }
    }
}


void resetExecutedFlagsIfNewDay() {
    static int lastDay = -1;

    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    int currentDay = timeinfo->tm_mday;

    if (currentDay != lastDay) {
        Serial.println("New day detected, resetting executed flags for all schedules.");
        for (int i = 0; i < scheduleCount; i++) {
            if (!schedules[i].days.isEmpty()) {
                // Reset executed flag for recurring schedules
                schedules[i].executed = false;

                // Update Adafruit IO to reset the executed flag for this schedule
                updateScheduleInAdafruitIO(schedules[i].id, false, schedules[i].enabled, schedules[i].time, schedules[i].days, schedules[i].device, schedules[i].scheduledDispenses);
            }
        }
        lastDay = currentDay;
    }
}

bool compareSchedulesForChanges(const Schedule &localSchedule, const Schedule &newSchedule) {
  // Compare time, days, and enabled status for changes
  return (localSchedule.time == newSchedule.time) && 
         (localSchedule.days == newSchedule.days) && 
         (localSchedule.enabled == newSchedule.enabled);
}

void monitorScheduleChanges() {
    // Set up a timer to check for changes periodically
    static unsigned long lastFetchTime = 0;
    const unsigned long fetchInterval = 30000;  // Fetch every 30 seconds

    if (millis() - lastFetchTime >= fetchInterval) {
        lastFetchTime = millis();
        
        // Fetch latest schedules from Adafruit IO
        fetchSchedulesFromAdafruitIO();

        // Compare fetched schedules with local schedules
        for (int i = 0; i < scheduleCount; i++) {
            int fetchedIndex = findScheduleByID(schedules[i].id);
            
            if (fetchedIndex != -1) {
                Schedule fetchedSchedule = schedules[fetchedIndex];

                // Check if the fetched schedule is different from the local schedule
                if (!compareSchedulesForChanges(schedules[i], fetchedSchedule)) {
                    Serial.printf("Detected changes in Schedule ID %s. Updating local schedule.\n", schedules[i].id.c_str());
                    
                    // Update the local schedule with new values
                    schedules[i].time = fetchedSchedule.time;
                    schedules[i].days = fetchedSchedule.days;
                    schedules[i].enabled = fetchedSchedule.enabled;
                    schedules[i].device = fetchedSchedule.device;
                    schedules[i].scheduledDispenses = fetchedSchedule.scheduledDispenses;
                    
                    // If necessary, update execution flags or trigger any actions based on changes
                    if (fetchedSchedule.enabled != schedules[i].enabled || fetchedSchedule.scheduledDispenses != schedules[i].scheduledDispenses) {
                        Serial.println("Executing actions based on updated schedule.");
                        checkScheduleAndControlDevices();
                    }
                }
            }
        }
    }
}

