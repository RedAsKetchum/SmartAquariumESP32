double avergearray(int* arr, int number) {
  int i, max, min;
  double avg;
  long amount = 0;

  if (number <= 0) {
    Serial.println("Error: Invalid array size!");
    return 0;
  }

  if (number < 5) {  // Direct calculation for small arrays
    for (i = 0; i < number; i++) {
      amount += arr[i];
    }
    avg = amount / number;
    return avg;
  } else {
    if (arr[0] < arr[1]) {
      min = arr[0];
      max = arr[1];
    } else {
      min = arr[1];
      max = arr[0];
    }
    for (i = 2; i < number; i++) {
      if (arr[i] < min) {
        amount += min;
        min = arr[i];
      } else if (arr[i] > max) {
        amount += max;
        max = arr[i];
      } else {
        amount += arr[i];
      }
    }
    avg = (double)amount / (number - 2);  // Exclude max and min
    return avg;
  }
}