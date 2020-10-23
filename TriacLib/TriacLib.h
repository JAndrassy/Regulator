
#ifndef _TRIAC_LIB_H_
#define _TRIAC_LIB_H_

//AVR
// zero-crossing pin must be a pin with external interrupt support (INTx)
// INTx: Uno/Nano/Mini 2,3; Mega 2,3,18-21; Badio 2,3,8
// TRIAC_PIN must be 16 bit timer's OCRxx pin (A, B or C)
// OCRxx Uno/Nano/Mini Timer1 pins 9,10(CS!); Mega: Timer1 11,12,13 Timer3 5,2,3; Timer4 6,7,8; Timer5 46,45,44; Badio Timer1 5,4(SD!); Timer3 12,13(SPI!)

//SAMD21 EVSYS 'users' can be only TCC0 and TCC1. TRIAC_PIN must be one of WO pins of this timers
// we use the Arduino pin-to-timer mapping from variant.cpp, which doesn't map all possible mux options
// M0 TCC0 pins 2,3,6,7 TCC1 pins 8,9
// MKR Zero TCC0 pins A3,A4,6,7 TCC1 2,3
// Zero TCC0 pins 3,4,6,7,12 TCC1 8,9
// Nano 33 IoT TCC0 pins 5,6,9,10 TCC1 pin 4

#ifdef ARDUINO_ARCH_SAMD
#include <wiring_private.h> // for pinPeripheral() as shown in https://www.arduino.cc/en/Tutorial/SamdSercom
#endif

namespace Triac {

volatile bool zeroCrossingFlag;
bool stopped = true;

const unsigned long AC_WAVE_MICROS = 10000; // at 50 Hz
const int TIMER_PRESCALER = 8;

#ifdef ARDUINO_ARCH_AVR
volatile uint8_t *tccrA;
uint8_t pinModeMask;
volatile uint16_t *tcnt;
volatile uint16_t *ocr;
#endif

#ifdef ARDUINO_ARCH_SAMD
const unsigned long PULSE_PERIOD = ((F_CPU / 1000000) * 30) / TIMER_PRESCALER;

Tcc* TCC;
uint8_t ulExtInt;

void syncTCC() {
  while (TCC->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);
}

#endif

void zeroCrossing() {
#ifdef ARDUINO_ARCH_AVR
  *tcnt = 0; // restart the timer
#endif
  zeroCrossingFlag = true;
}

//ICR1 = topPeriod; // fast PWM with ICR as top = set WGM11, WGM12, WGM13
//TCCR1A = _BV(WGM11) | _BV(COM1A0) | _BV(COM1A1); // set OC1A on Compare Match HIGH
//TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS11); // prescaler 8
#define CONFIGURE_TIMER(t,p) \
    ICR##t = topPeriod; \
    TCCR##t##A = _BV(WGM##t##1); \
    TCCR##t##B = _BV(WGM##t##2) | _BV(WGM##t##3) | _BV(CS##t##1); \
    ocr = &OCR##t##p; \
    tcnt = &TCNT##t; \
    pinModeMask = _BV(COM##t##p##0) | _BV(COM##t##p##1); \
    tccrA = &TCCR##t##A; \

void setup(byte zcPin, byte triacPin) {

#ifdef ARDUINO_ARCH_AVR
  attachInterrupt(digitalPinToInterrupt(zcPin), zeroCrossing, RISING);

  pinMode(triacPin, OUTPUT);

  uint32_t topPeriod = ((F_CPU / 1000000)* (AC_WAVE_MICROS - 300)) / TIMER_PRESCALER;

  uint8_t timerPin = digitalPinToTimer(triacPin);
#ifdef ARDUINO_AVR_MEGA2560
  if (timerPin == TIMER0A) {
    timerPin = TIMER1C;
  }
#endif
  switch (timerPin) {
  case TIMER1A:
#if defined(TCCR1A) && defined(COM1A1)
    CONFIGURE_TIMER(1, A)
#endif
    break;
  case TIMER1B:
#if defined(TCCR1A) && defined(COM1B1)
    CONFIGURE_TIMER(1, B)
#endif
    break;
  case TIMER1C:
#if defined(TCCR1A) && defined(COM1C1)
    CONFIGURE_TIMER(1, C)
#endif
    break;
  case TIMER3A:
#if defined(TCCR3A) && defined(COM3A1)
    CONFIGURE_TIMER(3, A)
#endif
    break;
  case TIMER3B:
#if defined(TCCR3A) && defined(COM3B1)
    CONFIGURE_TIMER(3, B)
#endif
    break;
  case TIMER3C:
#if defined(TCCR3A) && defined(COM3C1)
    CONFIGURE_TIMER(3, C)
#endif
    break;
  case TIMER4A:
#if defined(TCCR4A) && defined(COM4A1)
    CONFIGURE_TIMER(4, A)
#endif
    break;
  case TIMER4B:
#if defined(TCCR4A) && defined(COM4B1)
    CONFIGURE_TIMER(4, B)
#endif
    break;
  case TIMER4C:
#if defined(TCCR4A) && defined(COM4C1)
    CONFIGURE_TIMER(4, C)
#endif
    break;
  case TIMER5A:
#if defined(TCCR5A) && defined(COM5A1)
    CONFIGURE_TIMER(5, A)
#endif
    break;
  case TIMER5B:
#if defined(TCCR5A) && defined(COM5B1)
    CONFIGURE_TIMER(5, B)
#endif
    break;
  case TIMER5C:
#if defined(TCCR5A) && defined(COM5C1)
    CONFIGURE_TIMER(5, C)
#endif
    break;
  }
  *ocr = topPeriod; // must be set here to have pin LOW on start
#endif

#ifdef ARDUINO_ARCH_SAMD

  // setup the external interrupt
  attachInterrupt(zcPin, zeroCrossing, RISING);
  ulExtInt = g_APinDescription[zcPin].ulExtInt;
  EIC->EVCTRL.reg |= (1 << ulExtInt);// enable event
  EIC->INTENCLR.reg = EIC_INTENCLR_EXTINT(1 << ulExtInt); // turn-off interrupt

  const PinDescription& pinDesc = g_APinDescription[triacPin]; // Arduino pin description
  TCC = (Tcc*) GetTC(pinDesc.ulPWMChannel);
  uint8_t tcChannel = GetTCChannelNumber(pinDesc.ulPWMChannel);
  bool periF = (pinDesc.ulPinAttribute & PIN_ATTR_TIMER_ALT);

  // setup the pin as TCC wave out pin
  pinPeripheral(triacPin, periF ? PIO_TIMER_ALT : PIO_TIMER);

  // setup the timer

  REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TCC0_TCC1)); // assign clock
  while ( GCLK->STATUS.bit.SYNCBUSY == 1 );

  TCC->CTRLA.bit.SWRST = 1; // reset timer
  syncTCC();
  TCC->WAVE.reg |= TCC_WAVE_WAVEGEN_NPWM; // normal PWM as wave output mode
  syncTCC();
  TCC->CTRLBSET.reg = TCC_CTRLBSET_ONESHOT | TCC_CTRLBSET_DIR; //one shot and count down
  syncTCC();

  TCC->PER.reg = ((F_CPU / 1000000) * AC_WAVE_MICROS) / TIMER_PRESCALER; // initial value of period (triac off)
  syncTCC();
  TCC->CC[tcChannel].reg = PULSE_PERIOD; // pin on at compare match and off at 0 (timer counts down)
  syncTCC();

  TCC->EVCTRL.reg |= TCC_EVCTRL_TCEI0 | TCC_EVCTRL_EVACT0_RETRIGGER; // retrigger on event
  syncTCC();
  TCC->CTRLA.reg |= TCC_CTRLA_PRESCALER_DIV8 | TCC_CTRLA_ENABLE; // set prescaler and enable
  syncTCC();

  // event system
  PM->APBCMASK.reg |= PM_APBCMASK_EVSYS; // power it on
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(EVSYS_GCLK_ID_0) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(0); // assign a clock
  while (GCLK->STATUS.bit.SYNCBUSY);

  EVSYS->CTRL.bit.SWRST = 1; // reset
  while(EVSYS->CTRL.bit.SWRST);

  EVSYS->USER.reg = ((TCC == GetTC(0)) ? EVSYS_ID_USER_TCC0_EV_0 : EVSYS_ID_USER_TCC1_EV_0) | EVSYS_USER_CHANNEL(1); // set user TCC0 event 0 on channel 0
#endif
}

/*
 * r is from interval <0.0, 1.0>
 */
void setPeriod(float r) {
  if (r < 0.05) {
#ifdef ARDUINO_ARCH_AVR
    *tccrA &= ~pinModeMask; // disconnect pin
#endif
#ifdef ARDUINO_ARCH_SAMD
    EVSYS->CHANNEL.reg = EVSYS_CHANNEL_CHANNEL(0) | EVSYS_CHANNEL_EVGEN(0); // no event source
    while (!EVSYS->CHSTATUS.bit.USRRDY0);
#endif
    stopped = true;
  } else if (stopped) {
#ifdef ARDUINO_ARCH_AVR
    *tccrA |= pinModeMask;  // connect pin
#endif
#ifdef ARDUINO_ARCH_SAMD
    EVSYS->CHANNEL.reg = EVSYS_CHANNEL_CHANNEL(0) | EVSYS_CHANNEL_PATH_ASYNCHRONOUS | // channel 0 is async
        EVSYS_CHANNEL_EDGSEL_FALLING_EDGE | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_0 + ulExtInt); // source is ext.interrupt
    while (!EVSYS->CHSTATUS.bit.USRRDY0);
#endif
    stopped = false;
  }
  if (r > 0.95) {
    r = 0.95;
  }
  unsigned period = ((F_CPU / 1000000) * ((1.0 - r) * AC_WAVE_MICROS)) / TIMER_PRESCALER;

#ifdef ARDUINO_ARCH_AVR
  *ocr = period;
#endif
#ifdef ARDUINO_ARCH_SAMD
  TCC->PER.reg = period + PULSE_PERIOD;
  syncTCC();
#endif
}

void waitZeroCrossing() {
  unsigned long startMillis = millis();
  Triac::zeroCrossingFlag = false;
#ifdef ARDUINO_ARCH_SAMD
  EIC->INTENSET.reg = EIC_INTENSET_EXTINT(1 << ulExtInt);
#endif
  while (!Triac::zeroCrossingFlag && millis() - startMillis < (AC_WAVE_MICROS / 1000));
#ifdef ARDUINO_ARCH_SAMD
  EIC->INTENCLR.reg = EIC_INTENCLR_EXTINT(1 << ulExtInt);
  delayMicroseconds(100); // detector is too early and MKR is fast
#endif
}

}
#endif
