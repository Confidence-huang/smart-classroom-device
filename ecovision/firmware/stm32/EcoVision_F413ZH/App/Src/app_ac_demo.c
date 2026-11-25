/* Drive the demo receiver through the ZJ-IR-V2 module's AFN 22H raw waveform command. */
#include "app_ac_demo.h"
#include "app_log.h"
#include "ir_pulse_codec.h"
#include "main.h"

static UART_HandleTypeDef infrared;
static uint8_t address;
/* The network task reads capability. Only the startup code and output task can set it. */
static volatile bool ready;
static bool discovery_reported;
static uint32_t last_probe_at;

static uint8_t sum_bytes(const uint8_t *data, unsigned length)
{
    uint8_t sum = 0;
    for (unsigned i = 0; i < length; i++)
        sum = (uint8_t)(sum + data[i]);
    return sum;
}

/* AFN 06 only reads the module address, including on a retry. It sends no infrared command. */
static bool probe_address(uint32_t timeout_ms)
{
    uint8_t probe[] = {0x68, 0x07, 0x00, 0xff, 0x06, 0x05, 0x16};
    uint8_t reply[8];
    /* Discard a stale receive byte or overrun left by a late response. */
    __HAL_UART_CLEAR_OREFLAG(&infrared);
    if (HAL_UART_Transmit(&infrared, probe, sizeof probe, 20U) != HAL_OK ||
        HAL_UART_Receive(&infrared, reply, sizeof reply, timeout_ms) != HAL_OK) return false;
    if (reply[0] != 0x68 || reply[1] != 8 || reply[2] != 0 || reply[4] != 0x06 ||
        reply[3] != reply[5] || reply[5] == 0xff || reply[6] != sum_bytes(reply + 3, 3) ||
        reply[7] != 0x16) return false;
    address = reply[5];
    return true;
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
    if (HAL_UART_Init(&infrared) == HAL_OK) {
        HAL_Delay(400U); /* Allow the learning module to finish power-on. */
        ready = probe_address(250U);
    }
    last_probe_at = HAL_GetTick();
}

void AppAcDemo_Poll(uint32_t now)
{
    /* Retry a failed startup probe every 3 seconds, after the output task has written fan and lamp. */
    if (!ready && now - last_probe_at >= 3000U) {
        last_probe_at = now;
        if (HAL_UART_Init(&infrared) == HAL_OK && probe_address(40U)) {
            ready = true;
            discovery_reported = false;
        }
    }
    /* Report startup once and log the transition to ready without flooding COM15. */
    if (!discovery_reported) {
        discovery_reported = true;
        if (ready) AppLog_Printf("[ZJ-IR] UART5 ADDRESS_OK address=%02X\r\n", (unsigned)address);
        else AppLog_Printf("[ZJ-IR] UART5 no valid address reply; will retry\r\n");
    }
}

bool AppAcDemo_Ready(void)
{
    return ready;
}

AcModuleResult AppAcDemo_Send(uint8_t temperature_c)
{
    /* The output task only sends the fixed demo frame. Its stack need not hold a raw receive buffer. */
    uint16_t timings[IR_PULSE_DEMO_LENGTH];
    if (!ready || !IrPulse_EncodeDemoTimings(temperature_c ? 1U : 2U, temperature_c, timings))
        return AC_MODULE_REJECTED;

    /* Each 51-span project waveform needs at most two bytes per span at 8 µs resolution. */
    uint8_t packet[5U + 2U * IR_PULSE_DEMO_LENGTH + 2U] = {0x68, 0, 0, address, 0x22};
    unsigned length = 5;
    for (unsigned i = 0; i < IR_PULSE_DEMO_LENGTH; i++) {
        const unsigned ticks = (timings[i] + 4U) / 8U;
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
