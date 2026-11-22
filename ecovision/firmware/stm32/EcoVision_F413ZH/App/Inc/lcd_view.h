#ifndef LCD_VIEW_H
#define LCD_VIEW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app_environment.h"
enum {LCD_VIEW_WIDTH=160,LCD_VIEW_HEIGHT=128,LCD_VIEW_BYTES=160*128*2};
void LcdView_SetNetworkState(const char *state);
bool LcdView_Render(uint8_t *pixels,size_t size,bool linked,bool valid,uint8_t count,uint32_t seq);
bool LcdView_RenderDashboard(uint8_t *pixels,size_t size,bool linked,bool valid,uint8_t count,uint32_t seq,const EnvironmentSnapshot *env);
#endif
