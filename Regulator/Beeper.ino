
const int BEEP_1 = 4186;
const int BEEP_2 = 4699;

void beeperLoop() {

  const int ALARM_SOUND_INTERVAL = 3000; // 3 sec
  static unsigned long previousAlarmSoundMillis = 0;
  const int ALARM_REPEAT = 100; // 100 * 3 sec = 5 min
  static byte alarmCounter = 0;

  if (state == RegulatorState::ALARM) {
    if (loopStartMillis - previousAlarmSoundMillis > ALARM_SOUND_INTERVAL) {
      previousAlarmSoundMillis = loopStartMillis;
      if (alarmCounter < ALARM_REPEAT) {
        alarmSound();
        alarmCounter++;
      }
    }
  } else {
    alarmCounter = 0;
  }
}

void alarmSound() {
  Serial.println("ALARM");
  for (int i = 0; i < 3; i++) {
    tone(TONE_PIN, BEEP_1);
    delay(200);
    tone(TONE_PIN, BEEP_2);
    delay(200);
  }
  noTone(TONE_PIN);
}

void beep() {
  Serial.println("beep");
  tone(TONE_PIN, BEEP_1, 200);
}

