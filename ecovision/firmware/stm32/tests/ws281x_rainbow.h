#ifndef WS281X_RAINBOW_H
#define WS281X_RAINBOW_H

#include <stdbool.h>
#include <stdint.h>

enum { RAINBOW_PIXELS = 16 };

typedef struct {
    bool idle;
    bool candidate;
    bool stable;
    uint32_t changed_at;
} RainbowButton;

void Rainbow_Render(uint8_t frame[RAINBOW_PIXELS][3], unsigned head, uint8_t hue, bool reverse);
RainbowButton RainbowButton_Init(bool idle);
bool RainbowButton_Pressed(RainbowButton *button, bool level, uint32_t now);

#endif
