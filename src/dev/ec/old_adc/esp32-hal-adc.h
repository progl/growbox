
#include "esp32-hal.h"

uint16_t wega_analogRead(uint8_t pin);

bool wega_adcAttachPin(uint8_t pin);

bool wega_adcStart(uint8_t pin);

bool wega_adcBusy(uint8_t pin);

uint16_t wega_adcEnd(uint8_t pin);
