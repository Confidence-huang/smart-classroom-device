#ifndef OLED_SPI_H
#define OLED_SPI_H
#include <stdint.h>
enum { OLED_CLK, OLED_DATA, OLED_RESET, OLED_DC };
typedef struct {
    void (*pin)(unsigned id, unsigned high);
    void (*delay_ms)(unsigned ms);
} OledSpi;
void OledSpi_Reset(const OledSpi *io);
void OledSpi_Byte(const OledSpi *io, unsigned data, uint8_t value);
void OledSpi_Init(const OledSpi *io, unsigned sh1106);
void OledSpi_Text(const OledSpi *io, unsigned sh1106, unsigned page, const char *text);
/* Show a setpoint in Celsius. Pass zero to show OFF. */
void OledSpi_Setpoint(const OledSpi *io, unsigned sh1106, unsigned celsius);
#endif
