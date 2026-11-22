#ifndef APP_LIGHT_H
#define APP_LIGHT_H
#include <stdbool.h>
#include <stdint.h>
/* Current carrier photoresistor: PC5 / ADC1_IN15. Raw ADC, not lux.
 * Owned by the environment task; do not call concurrently on ADC1. */
void AppLight_Init(void);
bool AppLight_ReadRaw12(uint16_t *out);
#endif
