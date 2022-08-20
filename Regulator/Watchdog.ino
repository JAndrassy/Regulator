
#ifdef ARDUINO_ARCH_AVR
#include <avr/wdt.h>

const byte WDT_TIMER = WDTO_8S; // interrupt every 8 sec
const byte WDT_COUNT = 2;  // reset on second interrupt = 16 sec watchdog
volatile byte wdt_counter = 0;

void watchdogSetup() {
  byte bb = WDT_TIMER & 7; // WDP0, WDP1, WDP2 are first 3 bits
  if (WDT_TIMER > WDTO_1S) {
    bb |= _BV(WDP3); // WDP3 for 4 and 8 sec is bit 5
  }
  MCUSR &= ~_BV(WDRF); // Clear the watch dog reset
  WDTCSR |= _BV(WDCE) | _BV(WDE); // Set WD_change enable, set WD enable
  WDTCSR = bb | _BV(WDIE); // Set new watchdog timeout value and enable interrupt
}

void watchdogLoop() {
  wdt_counter = 0;
  wdt_reset();
}

void watchdogStop() {
  wdt_disable();
}

ISR(WDT_vect) {
  wdt_counter++;
  if (wdt_counter < WDT_COUNT) {
    wdt_reset(); // start timer again (we are still in interrupt-only mode)
  } else {
    if (state != RegulatorState::REST) {
      tone(TONE_PIN, BEEP_2, 400);
    }
    eventsWrite(WATCHDOG_EVENT, 0, 0);
    shutdown(); // to save events
    wdt_enable(WDTO_15MS); // self reset
  }
}
#endif

#ifdef ARDUINO_ARCH_SAMD

#include <WDTZero.h> // https://github.com/javos65/WDTZero

WDTZero wdt;

void watchdogSetup() {
  wdt.attachShutdown(watchdogShutdown);
  wdt.setup(WDT_SOFTCYCLE32S); // 32s because WiFiNINA connection timeout is 18 seconds
}

void watchdogLoop() {
  wdt.clear();
}

void watchdogStop() {
  wdt.setup(WDT_OFF);
}

void watchdogShutdown() {
  if (state != RegulatorState::REST) {
    tone(TONE_PIN, BEEP_2, 400);
  }
  eventsWrite(WATCHDOG_EVENT, 0, 0);
  shutdown();
}

#endif
