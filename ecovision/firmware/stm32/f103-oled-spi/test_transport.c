#include "oled_spi.h"
#include <assert.h>
#include <stdio.h>

static unsigned pins[4], clock_edges, captured, data_mode, delays;
static void pin(unsigned id, unsigned high) {
    if (id == OLED_CLK && high && !pins[OLED_CLK]) {
        captured = (captured << 1) | pins[OLED_DATA];
        data_mode = pins[OLED_DC];
        clock_edges++;
    }
    pins[id] = high;
}
static void delay(unsigned ms) { delays += ms; }
int main(void) {
    OledSpi io = {pin, delay};
    OledSpi_Reset(&io);
    assert(pins[OLED_RESET] == 1 && pins[OLED_CLK] == 0 && delays >= 20);
    OledSpi_Byte(&io, 0, 0xae);
    assert(clock_edges == 8 && captured == 0xae && data_mode == 0);
    assert(pins[OLED_CLK] == 0);
    captured = 0; clock_edges = 0;
    OledSpi_Byte(&io, 1, 0xa5);
    assert(clock_edges == 8 && captured == 0xa5 && data_mode == 1);
    assert(pins[OLED_CLK] == 0);
    puts("PASS: reset, SPI rising-edge MSB-first command/data, clock idle low");
    return 0;
}
