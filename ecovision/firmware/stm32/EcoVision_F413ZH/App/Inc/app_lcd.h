#ifndef APP_LCD_H
#define APP_LCD_H
#include <stdbool.h>
#include <stdint.h>
bool AppLcd_Init(void);
bool AppLcd_Show(bool linked,bool valid,uint8_t count,uint32_t seq);
#endif
