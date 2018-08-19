#ifdef ESP8266
#include <user_interface.h>

void watchdogSetup() {
  uint32_t reason = ESP.getResetInfoPtr()->reason;
  switch (reason) {
    case REASON_EXCEPTION_RST:
    case REASON_WDT_RST:
    case REASON_SOFT_WDT_RST:
      eventsWrite(WATCHDOG_EVENT, 0, reason);
      break;
    default:
      break;
  }
}

void watchdogLoop() {
}
#endif

#ifdef __AVR__
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

ISR(WDT_vect) {
  if (state != RegulatorState::REST) {
    beep();
  }
  wdt_counter++;
  eventsWrite(WATCHDOG_EVENT, wdt_counter, 0);
  if (wdt_counter < WDT_COUNT) {
    wdt_reset(); // start timer again (we are still in interrupt-only mode)
  } else {
    eventsLoop(); // to save events
    wdt_enable(WDTO_15MS); // self reset
  }
}
#endif

#ifdef ARDUINO_ARCH_SAMD

void watchdogSetup() {
  // One-time initialization of watchdog timer.
  // Insights from rickrlh and rbrucemtl in Arduino forum!

  // Generic clock generator 2, divisor = 32 (2^(DIV+1))
  GCLK->GENDIV.reg = GCLK_GENDIV_ID(2) | GCLK_GENDIV_DIV(4);
  // Enable clock generator 2 using low-power 32KHz oscillator.
  // With /32 divisor above, this yields 1024Hz(ish) clock.
  GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(2) |
                      GCLK_GENCTRL_GENEN |
                      GCLK_GENCTRL_SRC_OSCULP32K |
                      GCLK_GENCTRL_DIVSEL;
  while(GCLK->STATUS.bit.SYNCBUSY);
  // WDT clock = clock gen 2
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_WDT |
                      GCLK_CLKCTRL_CLKEN |
                      GCLK_CLKCTRL_GEN_GCLK2;

  // Enable WDT early-warning interrupt
  NVIC_DisableIRQ(WDT_IRQn);
  NVIC_ClearPendingIRQ(WDT_IRQn);
  NVIC_SetPriority(WDT_IRQn, 0); // Top priority
  NVIC_EnableIRQ(WDT_IRQn);

  WDT->INTENSET.bit.EW   = 1;      // Enable early warning interrupt
  WDT->EWCTRL.bit.EWOFFSET = 0xA;  // Early Warning Interrupt Time Offset 0xA
  WDT->CONFIG.bit.PER    = 0xB;   // Set period for chip reset 0xB 16384 clock cycles
  WDT->CTRL.bit.WEN      = 0;      // Disable window mode
  while(WDT->STATUS.bit.SYNCBUSY); // Sync CTRL write
  WDT->CTRL.bit.ENABLE = 1; // Start watchdog now!
  while(WDT->STATUS.bit.SYNCBUSY);
}

void watchdogLoop() {
  WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
  while(WDT->STATUS.bit.SYNCBUSY);
}

void WDT_Handler(void) {  // ISR for watchdog early warning, DO NOT RENAME!
  if (state != RegulatorState::REST) {
    tone(TONE_PIN, BEEP_1, 200);
  }
  eventsWrite(WATCHDOG_EVENT, 0, 0);
  shutdown();
  WDT->CLEAR.reg = 0xFF; // value different than WDT_CLEAR_CLEAR_KEY causes reset
  while(true);
}

#endif
