
This folder contains Calc sheets I use to determine and fine-tune the coefficients of the formulas for `power2period()` and for `elsensPower`. The coefficients mirror the electrical properties of my system.

The coefficients are determined and fine-tuned by comparing with power measured by external electricity meter. For basic settings a simple plug-in electricity meter can be used by writing down the measured value while setting the triac period with RegulatorDev sketch. For automatic RegulatorDev measurements and for fine-tune the coefficients with data from daily CSV logs, some smart electricity meter which can be read from sketch must be used. My Regulator source code has support for Belkin Wemo Inside Switch and for secondary meter on Fronius DataManager.

# Power to period calculation

Input for the triac period calculation is the surplus power available for heater. To consume the expected power a corresponding AC wave part must be cut out by setting the period to pulse the triac. The AC voltage rises and falls within the period so the period to power releation is not linear, it is an inclined cosine wave. To remove the wave, we use acos() and adjust the result with coefficients found by comparing the function output with table of power measured with external meter for different triac periods.

The PowerPilot algorithm works good without fine-tuned coefficients or even with a simple `period = availablePower / MAX_POWER` formula because it calculates the new `heatingPower` relative to previous. The better period calculation is good for initial `heatingPower` value calculation and for more realistic values of `heatingPower` variable in CSV logs.

dimmer-dev.ods works with data collected with RegulatorDev.ino sketch and/or RegulatorDev.java program and/or data manually written down from a simple plug-in electricity meter.

power2period.ods uses columns from daily CSV logs to fine-tune the coefficients with larger set of data. Prerequisite is an external meter which can be read from the Regulator sketch to log the values together with Regulator data.

# elsensPower

The elsensPower value is calculated from the current sensor reading. The formula's coefficients mirror the system properties of the AC part and the reading part: the MCU ADC or external ADC and of course the sensor.

The elsensPower calculation is not essential for Regulator. It has no function in the regulation algorithm. It is only logged and summed for statistics. I added the Grove Electricity Sensor in early stage of the project to check if the circulation pump is running and to detect if the heater was disconnected by the safety thermostat. Only later I attempted to use the value read from the sensor to calculate the consumed power. 

RegulatorDev.ods works with data collected with RegulatorDev.ino sketch and/or RegulatorDev.java program and/or data manually written down from a simple plug-in electricity meter.

else2power.ods uses columns from daily CSV logs to fine-tune the coefficients with larger set of data. Prerequisite is an external meter which can be read from the Regulator sketch to log the values together with Regulator data. 

The usual calculation of consumed power from value returned by analogRead consist of calculating the voltage on pin, then to calculate the AC current based on the characteristic of the sensor, then calculate RMS current from measurements over one or two AC waves and then multiply the RMS current with measured or fixed RMS voltage (230 V).

What I do: I sample the analogRead values 200 milliseconds (10 waves at 50 Hz) and calculate the RMS of this values. Then I multiply the RMS value with a coefficient determined to match the power measured by some external consumption meter. The coefficient covers the calculation of current from analogRead value and the multiplication with fixed RMS voltage.

The phase cutting technique changes [the power factor](https://en.wikipedia.org/wiki/Power_factor). For full power the power factor is 1. The more is cut away from the AC wave, the power factor is smaller then 1 (0.92 for example). The effective power is calculated as apparent power (V x I) multiplied by the power factor, which is a value between 0 and 1. Power factor is calculated as cosine of an angle.

The power factor changes with the amount of the current so I based the calculation of the power factor on the ratio of the RMS of sampled values to maximum RMS of sampled values. I determined the maximum angle for my system by matching in a Excel/Calc chart the lines of my function and of consumption measured by external meter. So the power factor is cosine of the part of the angle calculated with ratio of sensor values RMS.
