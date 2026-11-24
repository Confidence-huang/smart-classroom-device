#include "oled_spi.h"
#include "font.h"

void OledSpi_Reset(const OledSpi *io) {
    io->pin(OLED_CLK, 0); io->pin(OLED_DATA, 0); io->pin(OLED_DC, 0);
    io->pin(OLED_RESET, 1); io->delay_ms(10);
    io->pin(OLED_RESET, 0); io->delay_ms(10);
    io->pin(OLED_RESET, 1); io->delay_ms(100);
}
void OledSpi_Byte(const OledSpi *io, unsigned data, uint8_t value) {
    io->pin(OLED_DC, data ? 1 : 0);
    for (unsigned bit = 0; bit < 8; ++bit) {
        io->pin(OLED_CLK, 0);
        io->pin(OLED_DATA, (value & 0x80u) != 0);
        io->pin(OLED_CLK, 1);
        io->pin(OLED_CLK, 0);
        value = (uint8_t)(value << 1);
    }
}
static void cmd(const OledSpi *io, uint8_t v) { OledSpi_Byte(io, 0, v); }
static void cursor(const OledSpi *io, unsigned sh, unsigned page) {
    cmd(io, (uint8_t)(0xb0 | (page & 7)));
    cmd(io, sh ? 2 : 0); cmd(io, 0x10);
}
void OledSpi_Init(const OledSpi *io, unsigned sh) {
    static const uint8_t common[] = {
        0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0, 0x40,
        0xa1, 0xc8, 0xda, 0x12, 0x81, 0x7f, 0xd9, 0xf1,
        0xdb, 0x40, 0xa4, 0xa6
    };
    OledSpi_Reset(io);
    for (unsigned i = 0; i < sizeof common; ++i) cmd(io, common[i]);
    if (sh) { cmd(io, 0xad); cmd(io, 0x8b); }
    else { cmd(io, 0x8d); cmd(io, 0x14); cmd(io, 0x20); cmd(io, 0x02); }
    for (unsigned page = 0; page < 8; ++page) {
        cursor(io, sh, page);
        for (unsigned x = 0; x < 128; ++x) OledSpi_Byte(io, 1, 0);
    }
    cmd(io, 0xaf);
}
void OledSpi_Text(const OledSpi *io, unsigned sh, unsigned page, const char *text) {
    cursor(io, sh, page);
    unsigned x = 0;
    while (*text && x + 6 <= 128) {
        char c = *text++;
        unsigned i = 0;
        if (c == '-') i = 1;
        else if (c == '.') i = 2;
        else if (c == ':') i = 3;
        else if (c >= '0' && c <= '9') i = 4u + (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'Z') i = 14u + (unsigned)(c - 'A');
        for (unsigned col = 0; col < 5; ++col) OledSpi_Byte(io, 1, font5x7[i][col]);
        OledSpi_Byte(io, 1, 0); x += 6;
    }
    while (x++ < 128) OledSpi_Byte(io, 1, 0);
}
