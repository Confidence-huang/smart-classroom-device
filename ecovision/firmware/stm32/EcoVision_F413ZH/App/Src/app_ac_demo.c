/* Drive the demo receiver through the ZJ-IR-V2 module's AFN 22H raw waveform command. */
#include "app_ac_demo.h"
#include "ir_pulse_codec.h"
#include "main.h"

static UART_HandleTypeDef infrared;
static uint8_t address;
static bool ready;

static uint8_t sum_bytes(const uint8_t *data, unsigned length)
{
    uint8_t sum = 0;
    for (unsigned i = 0; i < length; i++)
        sum = (uint8_t)(sum + data[i]);
    return sum;
}

void AppAcDemo_Init(void)
{
    /* The existing UART5 wiring uses CN8 PC12 TX and PD2 RX at 3.3 V. */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_UART5_CLK_ENABLE();
    GPIO_InitTypeDef pin = {
        .Pin = GPIO_PIN_12, .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_LOW, .Alternate = GPIO_AF8_UART5
    };
    HAL_GPIO_Init(GPIOC, &pin);
    pin.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &pin);

    infrared = (UART_HandleTypeDef){
        .Instance = UART5, .Init = {
            .BaudRate = 115200, .WordLength = UART_WORDLENGTH_8B, .StopBits = UART_STOPBITS_1,
            .Parity = UART_PARITY_NONE, .Mode = UART_MODE_TX_RX, .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16
        }
    };
    if (HAL_UART_Init(&infrared) != HAL_OK) return;
    HAL_Delay(400U); /* Allow the learning module to finish power-on before read-only discovery. */
    uint8_t probe[] = {0x68, 0x07, 0x00, 0xff, 0x06, 0x05, 0x16};
    uint8_t reply[8];
    if (HAL_UART_Transmit(&infrared, probe, sizeof probe, 20U) != HAL_OK ||
        HAL_UART_Receive(&infrared, reply, sizeof reply, 250U) != HAL_OK) return;
    if (reply[0] != 0x68 || reply[1] != 8 || reply[2] != 0 || reply[4] != 0x06 ||
        reply[3] != reply[5] || reply[5] == 0xff || reply[6] != sum_bytes(reply + 3, 3) ||
        reply[7] != 0x16) return;
    address = reply[5];
    ready = true;
}

bool AppAcDemo_Ready(void)
{
    return ready;
}

AcModuleResult AppAcDemo_Send(uint8_t temperature_c)
{
    IrPulseFrame frame;
    if (!ready || !IrPulse_Encode24(temperature_c ? 1U : 2U, temperature_c, &frame))
        return AC_MODULE_REJECTED;

    /* Each 51-span project waveform needs at most two bytes per span at 8 µs resolution. */
    uint8_t packet[5U + 2U * IR_PULSE_DEMO_LENGTH + 2U] = {0x68, 0, 0, address, 0x22};
    unsigned length = 5;
    for (unsigned i = 0; i < frame.len; i++) {
        const unsigned ticks = (frame.us[i] + 4U) / 8U;
        if (ticks >= 128U) {
            packet[length++] = (uint8_t)((ticks & 127U) | 128U);
            packet[length++] = (uint8_t)(ticks >> 7U);
        } else packet[length++] = (uint8_t)ticks;
    }
    packet[1] = (uint8_t)(length + 2U);
    const uint8_t checksum = sum_bytes(packet + 3, length - 3U);
    packet[length++] = checksum;
    packet[length++] = 0x16;

    /* This reply is a module acknowledgement. F103 has no return link. */
    __HAL_UART_CLEAR_OREFLAG(&infrared);
    uint8_t reply[8];
    if (HAL_UART_Transmit(&infrared, packet, (uint16_t)length, 20U) != HAL_OK ||
        HAL_UART_Receive(&infrared, reply, sizeof reply, 150U) != HAL_OK)
        return AC_MODULE_UNKNOWN;
    if (reply[0] != 0x68 || reply[1] != 8 || reply[2] != 0 || reply[3] != address ||
        reply[4] != 0x01 || reply[6] != sum_bytes(reply + 3, 3) || reply[7] != 0x16)
        return AC_MODULE_UNKNOWN;
    return reply[5] == 0 ? AC_MODULE_ACK : AC_MODULE_REJECTED;
}
