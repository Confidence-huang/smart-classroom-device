#include "ws281x_rainbow.h"

#include <string.h>

static void wheel(uint8_t grb[3], uint8_t hue)
{
    if (hue < 85U)
    {
        grb[0] = (uint8_t)(hue * 3U);
        grb[1] = (uint8_t)(255U - grb[0]);
        grb[2] = 0U;
    }
    else if (hue < 170U)
    {
        grb[2] = (uint8_t)((hue - 85U) * 3U);
        grb[0] = (uint8_t)(255U - grb[2]);
        grb[1] = 0U;
    }
    else
    {
        grb[1] = (uint8_t)((hue - 170U) * 3U);
        grb[2] = (uint8_t)(255U - grb[1]);
        grb[0] = 0U;
    }
}

void Rainbow_Render(uint8_t frame[RAINBOW_PIXELS][3], unsigned head, uint8_t hue, bool reverse)
{
    const uint8_t brightness[] = {24U, 11U, 5U, 2U};
    memset(frame, 0, RAINBOW_PIXELS * sizeof frame[0]);
    for (unsigned distance = 0; distance < sizeof brightness; ++distance)
    {
        const unsigned offset = reverse ? distance : RAINBOW_PIXELS - distance;
        uint8_t *pixel = frame[(head + offset) % RAINBOW_PIXELS];
        wheel(pixel, (uint8_t)(hue - 18U * distance));
        for (unsigned color = 0; color < 3; ++color)
        {
            pixel[color] = (uint8_t)((pixel[color] * brightness[distance] + 127U) / 255U);
        }
    }
}

RainbowButton RainbowButton_Init(bool idle)
{
    const RainbowButton button = {idle, idle, idle, 0U};
    return button;
}

bool RainbowButton_Pressed(RainbowButton *button, bool level, uint32_t now)
{
    if (level != button->candidate)
    {
        button->candidate = level;
        button->changed_at = now;
    }
    if (button->candidate == button->stable || now - button->changed_at < 30U)
    {
        return false;
    }
    button->stable = button->candidate;
    return button->stable != button->idle;
}
