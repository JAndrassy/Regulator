
const int BEEP_1 = 2637;
const int BEEP_2 = 2794;

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
    beeperTone(BEEP_1, 200);
    beeperTone(BEEP_2, 200);
  }
}

void beep() {
  Serial.println("beep");
  beeperTone(BEEP_1, 200);
}

void beeperTone(int freq, uint32_t time) {
  tone(TONE_PIN, freq);
  delay(time);
  noTone(TONE_PIN);
}

