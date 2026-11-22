#include "app_ir.h"

#include "app_log.h"
#include "app_led.h"
#include "app_vision.h"
#include "app_net.h"
#include "cmsis_os.h"
#include "ir_protocol.h"

enum {
    RX_RING_SIZE = 64U,
    TX_TIMEOUT_MS = 25U,
    REPLY_TIMEOUT_MS = 250U,
    SEND_INTERVAL_MS = 500U,
    RX_RING_FULL = 0x10000U,
    RX_ARM_FAILED = 0x20000U
};

typedef struct {
    uint32_t tick;
    uint8_t byte;
} IrSample;

static UART_HandleTypeDef *ir_uart;
static volatile IrSample rx_ring[RX_RING_SIZE];
static volatile uint8_t rx_read;
static volatile uint8_t rx_write;
static volatile uint32_t rx_faults;
static uint8_t rx_byte;
static uint8_t tx_frame[IR_TX_SIZE];
static volatile bool tx_done;
static bool awaiting_reply;
static bool has_sent;
static uint32_t sent_at;
static IrParser parser;
static bool discarding;
static uint32_t discard_tick;

static void arm_rx(void)
{
    if (HAL_UART_Receive_IT(ir_uart, &rx_byte, 1U) != HAL_OK)
    {
        rx_faults |= RX_ARM_FAILED;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    AppLed_RxComplete(uart);
    AppVision_RxComplete(uart);
    AppNet_RxComplete(uart);
    if (uart != ir_uart)
    {
        return;
    }
    if (uart->ErrorCode != HAL_UART_ERROR_NONE)
    {
        rx_faults |= uart->ErrorCode;
        return;
    }

    const uint8_t write = rx_write;
    const uint8_t next = (uint8_t)((write + 1U) % RX_RING_SIZE);
    if (next == rx_read)
    {
        rx_faults |= RX_RING_FULL;
    }
    else
    {
        rx_ring[write].tick = HAL_GetTick();
        rx_ring[write].byte = rx_byte;
        rx_write = next;
    }
    arm_rx();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    AppLed_RxError(uart);
    AppVision_RxError(uart);
    AppNet_RxError(uart);
    if (uart == ir_uart)
    {
        rx_faults |= uart->ErrorCode;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart == ir_uart)
    {
        tx_done = true;
    }
}

static bool recover_rx(void)
{
    if (rx_faults == 0U)
    {
        return false;
    }

    /* No DMA is enabled. Mask only the IRQ servicing this UART. */
    HAL_NVIC_DisableIRQ(UART4_IRQn);
    const uint32_t faults = rx_faults;
    rx_faults = 0U;
    rx_read = rx_write;
    (void)HAL_UART_AbortReceive(ir_uart);
    arm_rx();
    HAL_NVIC_EnableIRQ(UART4_IRQn);

    parser.length = 0U;
    parser.truncated = false;
    discard_tick = HAL_GetTick();
    discarding = true;
    if (awaiting_reply)
    {
        (void)HAL_UART_AbortTransmit(ir_uart);
        awaiting_reply = false;
    }
    AppLog_Printf("[IR] UART/RX error 0x%lX; capture discarded\r\n", (unsigned long)faults);
    return true;
}

static void handle_event(const IrEvent *event)
{
    if (event->kind == IR_DECODED)
    {
        AppLog_Hex("[IR] RX NEC", event->bytes, event->length);
    }
    else if (event->kind == IR_ACK || event->kind == IR_REJECTED)
    {
        const bool matches_tx = awaiting_reply && tx_done &&
                                (event->last_byte_ms - sent_at < REPLY_TIMEOUT_MS);
        if (matches_tx)
        {
            awaiting_reply = false;
            AppLog_Hex(event->kind == IR_ACK ? "[IR] module accepted" : "[IR] module rejected",
                       event->bytes, event->length);
        }
        else
        {
            AppLog_Hex("[IR] unsolicited/late reply", event->bytes, event->length);
        }
    }
    else
    {
        AppLog_Hex(event->truncated ? "[IR] malformed (first bytes)" : "[IR] malformed",
                   event->bytes, event->length);
    }
}

static void process_rx(void)
{
    IrEvent event;
    const uint8_t stop = rx_write;
    while (rx_read != stop && rx_faults == 0U)
    {
        const uint8_t index = rx_read;
        const IrSample sample = rx_ring[index];
        rx_read = (uint8_t)((index + 1U) % RX_RING_SIZE);

        if (discarding)
        {
            discarding = sample.tick - discard_tick < IR_GAP_MS;
            discard_tick = sample.tick;
            if (discarding)
            {
                continue;
            }
        }
        if (IrParserPush(&parser, sample.byte, sample.tick, &event))
        {
            handle_event(&event);
        }
    }
    const uint32_t now = HAL_GetTick();
    if (rx_faults == 0U && rx_read == rx_write && IrParserPoll(&parser, now, &event))
    {
        handle_event(&event);
    }
}

static void send_test(uint32_t now)
{
    const uint8_t code[IR_CODE_SIZE] = {0x55, 0x55, 0x01};
    if (awaiting_reply || (has_sent && now - sent_at < SEND_INTERVAL_MS))
    {
        AppLog_Printf("[IR] busy; retry after 500 ms\r\n");
        return;
    }
    IrBuildTransmit(tx_frame, code);
    tx_done = false;
    sent_at = now;
    if (HAL_UART_Transmit_IT(ir_uart, tx_frame, sizeof tx_frame) != HAL_OK)
    {
        AppLog_Printf("[IR] TX could not start\r\n");
        return;
    }
    awaiting_reply = true;
    has_sent = true;
    AppLog_Hex("[IR] TX queued", tx_frame, sizeof tx_frame);
}

void AppIr_Run(UART_HandleTypeDef *module, UART_HandleTypeDef *console)
{
    /* The heartbeat task prints its startup line first on the shared console. */
    osDelay(650U);
    ir_uart = module;
    arm_rx();
    AppLog_Printf("[IR] UART4 9600 8N1; send ASCII T for one NEC test\r\n");

    for (;;)
    {
        if (!recover_rx())
        {
            process_rx();
        }

        const uint32_t now = HAL_GetTick();
        if (awaiting_reply && !tx_done && now - sent_at >= TX_TIMEOUT_MS)
        {
            (void)HAL_UART_AbortTransmit(ir_uart);
            awaiting_reply = false;
            AppLog_Printf("[IR] UART TX timeout\r\n");
        }
        else if (awaiting_reply && now - sent_at >= REPLY_TIMEOUT_MS + IR_GAP_MS)
        {
            awaiting_reply = false;
            AppLog_Printf("[IR] module reply timeout (250 ms)\r\n");
        }

        uint8_t key;
        if (HAL_UART_Receive(console, &key, 1U, 0U) == HAL_OK && key == 'T')
        {
            send_test(now);
        }
        osDelay(2U);
    }
}
