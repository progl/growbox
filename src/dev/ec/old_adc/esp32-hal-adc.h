
#include "esp32-hal.h"

 

/*
 * Get ADC value for pin
 * */
uint16_t wega_analogRead(uint8_t pin);

/*
 * Set the resolution of analogRead return values. Default is 12 bits (range from 0 to 4096).
 * If between 9 and 12, it will equal the set hardware resolution, else value will be shifted.
 * Range is 1 - 16
 *
 * Note: compatibility with Arduino SAM
 */
// void wega_analogReadResolution(uint8_t bits);

/*
 * Sets the sample bits and read resolution
 * Default is 12bit (0 - 4095)
 * Range is 9 - 12
 * */
// void wega_analogSetWidth(uint8_t bits);

/*
 * Set number of cycles per sample
 * Default is 8 and seems to do well
 * Range is 1 - 255
 * */
// void wega_analogSetCycles(uint8_t cycles);

/*
 * Set number of samples in the range.
 * Default is 1
 * Range is 1 - 255
 * This setting splits the range into
 * "samples" pieces, which could look
 * like the sensitivity has been multiplied
 * that many times
 * */
// void wega_analogSetSamples(uint8_t samples);

/*
 * Set the divider for the ADC clock.
 * Default is 1
 * Range is 1 - 255
 * */
// void wega_analogSetClockDiv(uint8_t clockDiv);

/*
 * Set the attenuation for all channels
 * Default is 11db
 * */
// void wega_analogSetAttenuation(adc_attenuation_t attenuation);

/*
 * Set the attenuation for particular pin
 * Default is 11db
 * */
// void wega_analogSetPinAttenuation(uint8_t pin, adc_attenuation_t attenuation);

/*
 * Get value for HALL sensor (without LNA)
 * connected to pins 36(SVP) and 39(SVN)
 * */
// int wega_hallRead();

/*
 * Non-Blocking API (almost)
 *
 * Note: ADC conversion can run only for single pin at a time.
 *       That means that if you want to run ADC on two pins on the same bus,
 *       you need to run them one after another. Probably the best use would be
 *       to start conversion on both buses in parallel.
 * */

/*
 * Attach pin to ADC (will also clear any other analog mode that could be on)
 * */
bool wega_adcAttachPin(uint8_t pin);

/*
 * Start ADC conversion on attached pin's bus
 * */
bool wega_adcStart(uint8_t pin);

/*
 * Check if conversion on the pin's ADC bus is currently running
 * */
bool wega_adcBusy(uint8_t pin);

/*
 * Get the result of the conversion (will wait if it have not finished)
 * */
uint16_t wega_adcEnd(uint8_t pin);

 

 