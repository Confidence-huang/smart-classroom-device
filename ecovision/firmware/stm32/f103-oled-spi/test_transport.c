#include "oled_spi.h"
#include <assert.h>
#include <stdio.h>

/* Record each command/data byte from the SPI pins for four full display pages. */
enum { PAGE_BYTES = 128, ADDRESS_BYTES = 3, DISPLAY_PAGES = 4 };
static uint8_t bytes[DISPLAY_PAGES * (PAGE_BYTES + ADDRESS_BYTES)];
static unsigned pins[4], clock_edges, captured, data_mode, delays;
static void pin(unsigned id, unsigned high) {
    if (id == OLED_CLK && high && !pins[OLED_CLK]) {
        captured = (captured << 1) | pins[OLED_DATA];
        data_mode = pins[OLED_DC];
        clock_edges++;
        if (clock_edges % 8 == 0) {
            const unsigned index = clock_edges / 8 - 1;
            assert(index < sizeof bytes);
            bytes[index] = (uint8_t)captured;
            captured &= 0xffu;
        }
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
    /* Decode the SSD1306 address commands and check the OFF bitmap. */
    clock_edges = 0;
    OledSpi_Setpoint(&io, 0, 0);
    assert(clock_edges == sizeof bytes * 8u);
    assert(bytes[0] == 0xb2 && bytes[1] == 0 && bytes[2] == 0x10);
    assert(bytes[ADDRESS_BYTES + 27] == 0 && bytes[ADDRESS_BYTES + 28] == 0xf0);
    assert(bytes[ADDRESS_BYTES + 36] == 0x0f); /* Top row of O */
    assert(bytes[ADDRESS_BYTES + 99] == 0 && bytes[ADDRESS_BYTES + 100] == 0);

    /* Temperature redraw covers the old label and includes the degree ring. */
    clock_edges = 0;
    OledSpi_Setpoint(&io, 1, 21);
    assert(clock_edges == sizeof bytes * 8u);
    assert(bytes[0] == 0xb2 && bytes[1] == 2 && bytes[2] == 0x10);
    assert(bytes[ADDRESS_BYTES + 15] == 0 && bytes[ADDRESS_BYTES + 16] == 0xf0); /* 2 */
    assert(bytes[ADDRESS_BYTES + 68] == 0xf0 && bytes[ADDRESS_BYTES + 72] == 0x0f);
    assert(bytes[ADDRESS_BYTES + 112] == 0 && bytes[ADDRESS_BYTES + 127] == 0);
    assert(bytes[3 * (PAGE_BYTES + ADDRESS_BYTES)] == 0xb5);

    puts("PASS: reset, SPI pins, OFF and 21 C bitmaps, SSD1306 and SH1106 addresses");
    return 0;
}
