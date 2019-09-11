#include <TriacLib.h>

const byte TRIAC_PIN = 6;
const byte ZC_EI_PIN = 5;

void setup() {

  Triac::setup(ZC_EI_PIN, TRIAC_PIN);
}

void loop() {
  // gradually increase brightness over time
  for(float brightness = 0.05; brightness < 1.00; brightness += 0.01){

    Triac::setPeriod(brightness);

    delay(20);
  }

  // and back down - decrease brightness over time
  for(float brightness = 1.00; brightness > 0.05; brightness -= 0.01){

    Triac::setPeriod(brightness);

    delay(20);
  }

}
