
void buttonSetup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void buttonLoop() {

  const int LONG_PRESS_INTERVAL = 5000; // 5 sec
  const int TOO_LONG_PRESS_INTERVAL = 2 * LONG_PRESS_INTERVAL;
  static unsigned long pressStartMillis = 0;
  static boolean longPressDone = false;

  boolean pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (!buttonPressed) { // could be set from Blynk, Web, Telnet
    buttonPressed = pressed; // set the global 'virtual' button press
  }

  // long press handling
  if (pressStartMillis == 0) { // not counting
    if (pressed) { // start counting
      pressStartMillis = loopStartMillis;
    }
  } else if (!pressed) { // stop counting
     pressStartMillis = 0;
     longPressDone = false;
  } else { // counting
    unsigned long d = loopStartMillis - pressStartMillis;
    if (d > TOO_LONG_PRESS_INTERVAL) { // button sticked?
      heatingPower = 0;
      alarmSound();
    } else if (!longPressDone && d > LONG_PRESS_INTERVAL) { // long press detected
      beep();
      manualRunRequest = true;
      longPressDone = true; // because manualRunRequest is cleared after activation
    }
  }
}
