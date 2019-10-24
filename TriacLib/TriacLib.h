
#ifndef _TRIAC_LIB_H_
#define _TRIAC_LIB_H_

//AVR
// zero-crossing pin must be a pin with external interrupt support (INTx)
// INTx: Uno/Nano/Mini 2,3; Mega 2,3, 18-21; Badio 2,3,8
// TRIAC_PIN must be 16 bit timer's OCRx pin (A or B)
// OCRx Uno/Nano/Mini Timer1 pins 9, 10(CS!); Mega: Timer1 11,12 Timer3 5,2; Timer4 6,7; Timer5 46; Badio Timer1 5,4(SD!); Timer3 12,13 (SPI!)
// only Timer 1 OCR1A is implemented

//SAMD21 EVSYS 'users' can be only TCC0 and TCC1. TRIAC_PIN must be one of WO pins of this timers
// M0 TCC0 pins 2,3,6,7 TCC1 pins 8,9
// MKR Zero TCC0 pins A3,A4,6,7 TCC1 2,3
// Zero TCC0 pins 3,4,6,7,12 TCC1 8,9
// only triacPin 6 is implemented (PORT_PMUX_PMUXE_F)

namespace Triac {

const unsigned long AC_WAVE_MICROS = 10000; // at 50 Hz
const int TIMER_PRESCALER = 8;

#ifdef ARDUINO_ARCH_SAMD
const unsigned long PULSE_PERIOD = ((F_CPU / 1000000) * 30) / TIMER_PRESCALER;

Tcc* TCC;

void syncTCC() {
  while (TCC->SYNCBUSY.reg & TCC_SYNCBUSY_MASK);
}

#endif

void zeroCrossing() {
#ifdef ARDUINO_ARCH_AVR
  TCNT1 = 0; // restart the timer
#endif
}

void setup(byte zcPin, byte triacPin) {

#ifdef ARDUINO_ARCH_AVR
  attachInterrupt(digitalPinToInterrupt(zcPin), zeroCrossing, RISING);

  pinMode(triacPin, OUTPUT);

  uint32_t topPeriod = ((F_CPU / 1000000)* (AC_WAVE_MICROS - 300)) / TIMER_PRESCALER;
  ICR1 = topPeriod; // fast PWM with ICR as top = set WGM11, WGM12, WGM13
  TCCR1A = _BV(WGM11) | _BV(COM1A0) | _BV(COM1A1); // set OC1A on Compare Match HIGH
  TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS11); // prescaler 8
  OCR1A = topPeriod; // must be set here to have pin LOW on start
#endif

#ifdef ARDUINO_ARCH_SAMD

  // setup the external interrupt
  attachInterrupt(zcPin, zeroCrossing, RISING);
  uint8_t ulExtInt = g_APinDescription[zcPin].ulExtInt;
  EIC->EVCTRL.reg |= (1 << ulExtInt);// enable event
  EIC->INTENCLR.reg = EIC_INTENCLR_EXTINT(1 << ulExtInt); // turn-off interrupt

  const PinDescription& pinDesc = g_APinDescription[triacPin]; // Arduino pin description
  TCC = (Tcc*) GetTC(pinDesc.ulPWMChannel);
  uint8_t tcChannel = GetTCChannelNumber(pinDesc.ulPWMChannel);

  // setup the pin as TCC wave out pin
  PORT->Group[pinDesc.ulPort].PINCFG[pinDesc.ulPin].bit.PMUXEN = 1;
  PORT->Group[pinDesc.ulPort].PMUX[pinDesc.ulPin >> 1].reg |= PORT_PMUX_PMUXE_F; // pin 6 only

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

  EVSYS->USER.reg = EVSYS_ID_USER_TCC0_EV_0 | EVSYS_USER_CHANNEL(1); // set user TCC0 event 0 on channel 0
  EVSYS->CHANNEL.reg = EVSYS_CHANNEL_CHANNEL(0) | EVSYS_CHANNEL_PATH_ASYNCHRONOUS | // channel 0 is async
      EVSYS_CHANNEL_EDGSEL_FALLING_EDGE | EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_0 + ulExtInt); // source is ext.interrupt
  while (!EVSYS->CHSTATUS.bit.USRRDY0);
#endif
}

/*
 * r is from interval <0.0, 1.0>
 */
void setPeriod(float r) {
  if (r > 0.95) {
    r = 0.95;
  }
  unsigned long period = ((F_CPU / 1000000) * ((1.0 - r) * AC_WAVE_MICROS)) / TIMER_PRESCALER;

#ifdef ARDUINO_ARCH_AVR
  OCR1A = period;
#endif
#ifdef ARDUINO_ARCH_SAMD
  TCC->PER.reg = period + PULSE_PERIOD;
  syncTCC();
#endif
}

}
#endif
