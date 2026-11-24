/* Standalone F103C8 screen diagnostic. No IR, networking or NUCLEO control. */
#include "stm32f1xx.h"
#include "oled_spi.h"
#ifndef OLED_SH1106
#define OLED_SH1106 0
#endif
static volatile uint32_t ticks;
void SysTick_Handler(void) { ++ticks; }
static void delay_ms(unsigned ms) {
    uint32_t start = ticks;
    while ((uint32_t)(ticks - start) < ms) __WFI();
}
static void pin(unsigned id, unsigned high) {
    const unsigned bit = 6 + id; /* PB6 clock, PB7 data, PB8 reset, PB9 DC */
    GPIOB->BSRR = high ? (1u << bit) : (1u << (bit + 16));
    /* Conservative bit-bang speed on HSI, no crystal/PLL dependency. */
    for (unsigned n = 0; n < 16; ++n) __NOP();
}
int main(void) {
    SystemCoreClockUpdate();
    if (SysTick_Config(SystemCoreClock / 1000u)) while (1) {}
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
    /* Set output latches before switching to 2 MHz push-pull GPIO mode. */
    GPIOB->BSRR = (1u << 8) | (1u << 22) | (1u << 23) | (1u << 25);
    GPIOB->CRL = (GPIOB->CRL & 0x00ffffffu) | 0x22000000u;
    GPIOB->CRH = (GPIOB->CRH & 0xffffff00u) | 0x22u;
    GPIOC->BSRR = 1u << 13;
    GPIOC->CRH = (GPIOC->CRH & ~(15u << 20)) | (2u << 20);
    const OledSpi io = {pin, delay_ms};
    delay_ms(200);
    OledSpi_Init(&io, OLED_SH1106);
    OledSpi_Text(&io, OLED_SH1106, 0, "SPI OLED TEST");
    OledSpi_Text(&io, OLED_SH1106, 2, "STM32F103 READY");
    OledSpi_Text(&io, OLED_SH1106, 4, OLED_SH1106 ? "SH1106 PROFILE" : "SSD1306 PROFILE");
    OledSpi_Text(&io, OLED_SH1106, 6, "IR NOT ENABLED");
    while (1) {
        GPIOC->BSRR = 1u << (13 + 16); delay_ms(500);
        GPIOC->BSRR = 1u << 13; delay_ms(500);
    }
}
