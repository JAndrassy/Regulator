
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
  pinMode(TONE_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    beeperTone(BEEP_1, 200);
    beeperTone(BEEP_2, 200);
  }
  pinMode(TONE_PIN, INPUT); // to reduce noise from amplifier
}

void beep() {
  Serial.println("beep");
  pinMode(TONE_PIN, OUTPUT);
  beeperTone(BEEP_1, 200);
  pinMode(TONE_PIN, INPUT); // to reduce noise from amplifier
}

void beeperTone(int freq, uint32_t time) {
  tone(TONE_PIN, freq);
  delay(time);
  noTone(TONE_PIN);
}

