/* F103C8 SPI OLED + wzy 24-bit demonstration receiver. */
#include "stm32f1xx.h"
#include "oled_spi.h"
#include "ir_receiver.h"
#include <stdio.h>
#ifndef OLED_SH1106
#define OLED_SH1106 0
#endif
static volatile uint32_t ticks;
typedef struct { uint32_t ms; uint16_t capture; uint8_t falling; } Edge;
static volatile Edge edges[128];
static volatile uint16_t head, tail;
static volatile unsigned capture_fault;
/* Read-only SWD diagnostics: magic,tick,edges,ok,bad,faults,PA0,on,temp,mode,fan,cmd,param. */
volatile uint32_t g_ir_debug[16];
static IrReceiver receiver;
void SysTick_Handler(void) { ++ticks; }
void TIM2_IRQHandler(void) {
    uint32_t status = TIM2->SR;
    if (status & TIM_SR_CC1OF) capture_fault = 1;
    if (status & TIM_SR_CC1IF) {
        uint16_t captured = (uint16_t)TIM2->CCR1;
        unsigned falling = (TIM2->CCER & TIM_CCER_CC1P) != 0;
        TIM2->CCER ^= TIM_CCER_CC1P;
        ++g_ir_debug[2];
        uint16_t next = (uint16_t)((head + 1) & 127);
        if (next == tail) capture_fault = 1;
        else {
            edges[head].capture = captured; edges[head].ms = ticks;
            edges[head].falling = (uint8_t)falling;
            __DMB(); head = next;
        }
    }
    TIM2->SR = ~(TIM_SR_CC1IF | TIM_SR_CC1OF);
}
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
static void receive_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    GPIOA->BSRR = 1; /* PA0 input pull-up, active-low demodulated receiver */
    GPIOA->CRL = (GPIOA->CRL & ~15u) | 8u;
    TIM2->CR1 = 0; TIM2->DIER = 0;
    TIM2->PSC = SystemCoreClock / 1000000u - 1u;
    TIM2->ARR = 0xffff; TIM2->CCMR1 = 1; /* CH1 maps TI1, no input filter */
    TIM2->CCER = TIM_CCER_CC1E | TIM_CCER_CC1P; /* start falling */
    TIM2->EGR = TIM_EGR_UG; TIM2->SR = 0;
    TIM2->DIER = TIM_DIER_CC1IE;
    NVIC_SetPriority(TIM2_IRQn, 1); NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}
static void receive_poll(void) {
    if (capture_fault) {
        __disable_irq();
        tail = head; capture_fault = 0;
        TIM2->CCER |= TIM_CCER_CC1P; /* reacquire from a falling edge */
        TIM2->SR = ~(TIM_SR_CC1IF | TIM_SR_CC1OF);
        __enable_irq();
        IrReceiver_Fault(&receiver);
    }
    while (tail != head) {
        Edge e;
        __DMB();
        e.ms = edges[tail].ms; e.capture = edges[tail].capture;
        e.falling = edges[tail].falling;
        __DMB(); tail = (uint16_t)((tail + 1) & 127);
        IrReceiver_Edge(&receiver, e.capture, e.ms, e.falling);
    }
    IrReceiver_Poll(&receiver, ticks);
    const IrState *s = &receiver.state;
    g_ir_debug[0] = 0x49525331u; g_ir_debug[1] = ticks;
    g_ir_debug[3] = s->ok; g_ir_debug[4] = s->bad; g_ir_debug[5] = s->faults;
    g_ir_debug[6] = (GPIOA->IDR & 1) != 0;
    g_ir_debug[7] = s->on; g_ir_debug[8] = s->temp; g_ir_debug[9] = s->mode;
    g_ir_debug[10] = s->fan; g_ir_debug[11] = s->cmd; g_ir_debug[12] = s->param;
}
static void draw(const OledSpi *io) {
    char line[24];
    const IrState *s = &receiver.state;
    OledSpi_Text(io, OLED_SH1106, 0, "IR RX READY");
    OledSpi_Text(io, OLED_SH1106, 1, !s->ok ? "AC DEMO: WAIT" : (s->on ? "AC DEMO: ON" : "AC DEMO: OFF"));
    (void)snprintf(line, sizeof line, "TEMP: %u C", s->temp);
    OledSpi_Text(io, OLED_SH1106, 2, line);
    (void)snprintf(line, sizeof line, "MODE:%u FAN:%u", s->mode, s->fan);
    OledSpi_Text(io, OLED_SH1106, 3, line);
    (void)snprintf(line, sizeof line, "RX:%lu BAD:%lu", (unsigned long)s->ok, (unsigned long)s->bad);
    OledSpi_Text(io, OLED_SH1106, 4, line);
    (void)snprintf(line, sizeof line, "CMD:%02X PAR:%u", s->cmd, s->param);
    OledSpi_Text(io, OLED_SH1106, 5, line);
    (void)snprintf(line, sizeof line, "PA0:%c E:%lu", (GPIOA->IDR & 1) ? 'H' : 'L', (unsigned long)g_ir_debug[2]);
    OledSpi_Text(io, OLED_SH1106, 6, line);
    (void)snprintf(line, sizeof line, "ERR:%lu UP:%lu", (unsigned long)s->faults, (unsigned long)(ticks / 1000u));
    OledSpi_Text(io, OLED_SH1106, 7, line);
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
    IrReceiver_Init(&receiver); receive_init();
    uint32_t shown = ticks - 500u;
    while (1) {
        receive_poll();
        GPIOC->BSRR = (ticks / 500u & 1u) ? (1u << 13) : (1u << (13 + 16));
        if ((uint32_t)(ticks - shown) >= 500u) { shown = ticks; draw(&io); }
    }
}
