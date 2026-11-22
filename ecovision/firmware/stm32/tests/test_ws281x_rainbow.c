#include "ws281x_rainbow.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint8_t frame[16][3];

    Rainbow_Render(frame, 0, 0, false);
    assert(frame[0][0] == 0 && frame[0][1] == 24 && frame[0][2] == 0);
    assert(frame[15][1] > frame[14][1] && frame[14][1] > frame[13][1]);
    assert(frame[1][0] == 0 && frame[1][1] == 0 && frame[1][2] == 0);
    Rainbow_Render(frame, 15, 85, true);
    assert(frame[15][0] == 24 && frame[15][1] == 0 && frame[15][2] == 0);
    assert(frame[0][0] > frame[1][0] && frame[1][0] > frame[2][0]);
    assert(frame[14][0] == 0 && frame[14][1] == 0 && frame[14][2] == 0);

    for (unsigned idle = 0; idle < 2; ++idle)
    {
        RainbowButton button = RainbowButton_Init(idle != 0);
        assert(!RainbowButton_Pressed(&button, !idle, 1));
        assert(!RainbowButton_Pressed(&button, idle != 0, 10));
        assert(!RainbowButton_Pressed(&button, !idle, 20));
        assert(!RainbowButton_Pressed(&button, !idle, 49));
        assert(RainbowButton_Pressed(&button, !idle, 50));
        assert(!RainbowButton_Pressed(&button, !idle, 1000));
        assert(!RainbowButton_Pressed(&button, idle != 0, 1010));
        assert(!RainbowButton_Pressed(&button, idle != 0, 1040));
        assert(!RainbowButton_Pressed(&button, !idle, 1050));
        assert(RainbowButton_Pressed(&button, !idle, 1080));
    }
    puts("PASS: GRB, four-pixel trail, reverse wrap, debounce, repeat presses and both B1 polarities");
    return 0;
}
