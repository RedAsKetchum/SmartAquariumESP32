void activateServo() {
  if (!servoActive) {
    // Move servo to 0 degrees (towards dispensing opening)
    myServo.write(0);
    servoActive = true;
    servoMoveStartTime = millis();  // Record when the movement started
    servoState = 1;  // Set state to "moving to 45 degrees"
  }
}

void handleServoMovement() {
  unsigned long currentMillis = millis();

  // Check the servo state
  if (servoState == 1 && currentMillis - servoMoveStartTime >= 500) {
    //  Half a second have passed, return the servo to the original position (57 degrees)
    myServo.write(57);
    servoMoveStartTime = currentMillis;  // Reset the timer for returning to 0 degrees 
    servoState = 2;  // Set state to "returning"
  } //else if (servoState == 2 && currentMillis - servoMoveStartTime >= 6000) {
    else if (servoState == 2) {
    // Servo has returned, reset to idle state
    servoState = 0;
    servoActive = false;
  }
}