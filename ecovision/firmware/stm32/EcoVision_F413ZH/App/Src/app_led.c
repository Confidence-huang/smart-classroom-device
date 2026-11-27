#include "app_led.h"
#include "app_log.h"
#include "app_fan.h"
#include "app_reed.h"
#include "app_servo.h"
#include "app_vision.h"
#include "app_control.h"
#include "cmsis_os.h"

#include <stdbool.h>

enum { LED_COUNT = 16, LED_BITS = 24, RESET_SLOTS = 320, RX_SIZE = 32 };
static volatile uint8_t actual_level;
uint8_t AppLed_GetLevel(void){return actual_level;}
static TIM_HandleTypeDef led_timer;
static DMA_HandleTypeDef led_dma;
static uint32_t duty[LED_COUNT * LED_BITS + RESET_SLOTS];
static UART_HandleTypeDef *command_uart;
static uint8_t rx_byte;
static volatile uint8_t rx_data[RX_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile bool rx_error;
static bool fan_prefix;
static uint32_t fan_prefix_at;
static uint8_t servo_digits_left;
static uint16_t servo_acc;
static uint32_t servo_prefix_at;
static ServoChannel bench_servo_ch = SERVO_CH_OPEN;
typedef enum { WIN_SEQ_IDLE = 0, WIN_SEQ_RETREAT, WIN_SEQ_ACTUATE, WIN_SEQ_RELEASE } WinSeqStep;
static struct {
    WinSeqStep step;
    bool target_open;
    uint32_t step_at;
} win_seq;
/* R, G, B. Warm pale yellow, scaled equally across four brightness steps. */
static const uint8_t levels[5][3] = {
    {0U, 0U, 0U},
    {8U, 6U, 2U},
    {24U, 18U, 6U},
    {48U, 36U, 12U},
    {96U, 72U, 24U}
};

static void receive_byte(void)
{
    if (HAL_UART_Receive_IT(command_uart, &rx_byte, 1U) != HAL_OK)
    {
        rx_error = true;
    }
}

void AppLed_RxComplete(UART_HandleTypeDef *uart)
{
    if (uart != command_uart)
    {
        return;
    }
    const uint8_t next = (uint8_t)((rx_head + 1U) % RX_SIZE);
    if (next == rx_tail || uart->ErrorCode != HAL_UART_ERROR_NONE)
    {
        rx_error = true;
    }
    else
    {
        rx_data[rx_head] = rx_byte;
        rx_head = next;
    }
    receive_byte();
}

void AppLed_RxError(UART_HandleTypeDef *uart)
{
    if (uart == command_uart)
    {
        rx_error = true;
    }
}

static bool initialize_output(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    led_timer.Instance = TIM2;
    led_timer.Init.Period = 119U;
    TIM_OC_InitTypeDef channel = {0};
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    if (HAL_TIM_PWM_Init(&led_timer) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&led_timer, &channel, TIM_CHANNEL_4) != HAL_OK ||
        HAL_TIM_PWM_Start(&led_timer, TIM_CHANNEL_4) != HAL_OK)
    {
        return false;
    }
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_11;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Speed = GPIO_SPEED_FREQ_HIGH;
    pin.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOB, &pin);
    led_dma.Instance = DMA1_Stream7;
    led_dma.Init.Channel = DMA_CHANNEL_3;
    led_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    led_dma.Init.MemInc = DMA_MINC_ENABLE;
    led_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    led_dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    led_dma.Init.Mode = DMA_NORMAL;
    led_dma.Init.Priority = DMA_PRIORITY_HIGH;
    return HAL_DMA_Init(&led_dma) == HAL_OK;
}

static bool show_level(uint8_t level)
{
    /* WS281x wire order is G, R, B. */
    const uint8_t grb[3] = {levels[level][1], levels[level][0], levels[level][2]};
    for (unsigned bit = 0; bit < LED_COUNT * LED_BITS; ++bit)
    {
        const uint8_t value = grb[(bit / 8U) % 3U];
        duty[bit] = (value & (0x80U >> (bit % 8U))) != 0U ? 70U : 37U;
    }
    if (HAL_DMA_Start(&led_dma, (uint32_t)duty, (uint32_t)&TIM2->CCR4,
                      (uint32_t)(sizeof duty / sizeof duty[0])) != HAL_OK)
    {
        return false;
    }
    __HAL_TIM_ENABLE_DMA(&led_timer, TIM_DMA_UPDATE);
    const HAL_StatusTypeDef result = HAL_DMA_PollForTransfer(&led_dma, HAL_DMA_FULL_TRANSFER, 5U);
    __HAL_TIM_DISABLE_DMA(&led_timer, TIM_DMA_UPDATE);
    if (result != HAL_OK)
    {
        (void)HAL_DMA_Abort(&led_dma);
        __HAL_TIM_SET_COMPARE(&led_timer, TIM_CHANNEL_4, 0U);
    }
    if(result==HAL_OK)actual_level=level;
    return result == HAL_OK;
}

void AppLed_Run(UART_HandleTypeDef *console)
{
    /* Let the heartbeat task finish the boot log on USART3. */
    osDelay(650U);
    if (!initialize_output() || !show_level(0U))
    {
        AppLog_Printf("[LED] ERROR initialization or OFF frame failed\r\n");
        return;
    }
    command_uart = console;
    receive_byte();
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    AppControl_Init();
    Servo_Init();
    bench_servo_ch = SERVO_CH_OPEN;
    AppLog_Printf("[CTRL] AUTO READY; B1 long=LOCAL_OFF short=AUTO; console 0/F0=safety OFF, V=handshake\r\n");
    AppLog_Printf("[BENCH] SERVO PE9(A=open)+PE11(B=close) init 0deg(500us); cmd: A/B, +/-, Z(0), M(45), S000..S180, O/C\r\n");
    uint32_t control_at=HAL_GetTick()-20U;

    for (;;)
    {
        if (win_seq.step != WIN_SEQ_IDLE)
        {
            const uint32_t now_seq = HAL_GetTick();
            const ServoChannel active_ch = win_seq.target_open ? SERVO_CH_OPEN : SERVO_CH_CLOSE;
            if (win_seq.step == WIN_SEQ_RETREAT && (uint32_t)(now_seq - win_seq.step_at) >= 400U)
            {
                Servo_SetChannelAngleDeg(active_ch, 45U);
                win_seq.step = WIN_SEQ_ACTUATE;
                win_seq.step_at = now_seq;
                AppLog_Printf("[WIN_BENCH] actuate ch=%u deg=45\r\n", (unsigned)active_ch + 1U);
            }
            else if (win_seq.step == WIN_SEQ_ACTUATE && (uint32_t)(now_seq - win_seq.step_at) >= 500U)
            {
                Servo_SetChannelAngleDeg(active_ch, 0U);
                win_seq.step = WIN_SEQ_IDLE;
                AppLog_Printf("[WIN_BENCH] done %s (returned 0deg)\r\n", win_seq.target_open ? "OPEN" : "CLOSE");
            }
        }
        if (fan_prefix && HAL_GetTick() - fan_prefix_at >= 500U)
        {
            fan_prefix = false;
            AppLog_Printf("[FAN] ERROR incomplete command; use F0..F4\r\n");
        }
        if (servo_digits_left > 0U && HAL_GetTick() - servo_prefix_at >= 1000U)
        {
            servo_digits_left = 0U;
            AppLog_Printf("[SERVO_BENCH] ERROR incomplete command; use S000..S180\r\n");
        }
        if (rx_error)
        {
            fan_prefix = false;
            servo_digits_left = 0U;
            HAL_NVIC_DisableIRQ(USART3_IRQn);
            rx_error = false;
            rx_tail = rx_head;
            (void)HAL_UART_AbortReceive(console);
            receive_byte();
            HAL_NVIC_EnableIRQ(USART3_IRQn);
            AppLog_Printf("[LED] ERROR UART RX; pending input discarded\r\n");
        }
        if (rx_tail != rx_head)
        {
            const uint8_t command = rx_data[rx_tail];
            rx_tail = (uint8_t)((rx_tail + 1U) % RX_SIZE);
            if (servo_digits_left > 0U)
            {
                if (command >= '0' && command <= '9')
                {
                    servo_acc = (uint16_t)(servo_acc * 10U + (uint16_t)(command - '0'));
                    servo_digits_left--;
                    if (servo_digits_left == 0U)
                    {
                        uint16_t deg = (servo_acc > 180U) ? 180U : servo_acc;
                        Servo_SetChannelAngleDeg(bench_servo_ch, deg);
                        AppLog_Printf("[SERVO_BENCH] ch=%u deg=%u pulse_us=%u\r\n",
                                      (unsigned)bench_servo_ch + 1U,
                                      (unsigned)Servo_GetChannelAngleDeg(bench_servo_ch),
                                      (unsigned)Servo_GetChannelPulseUs(bench_servo_ch));
                    }
                }
                else
                {
                    servo_digits_left = 0U;
                    AppLog_Printf("[SERVO_BENCH] ERROR expected 3 digits S000..S180\r\n");
                }
            }
            else if (fan_prefix)
            {
                fan_prefix = false;
                if (command >= '0' && command <= '4')
                {
                    if(command=='0')AppControl_SafetyOff();
                    else AppLog_Printf("[CTRL] REJECTED bench speed: controller owns outputs; B1 short resumes AUTO\r\n");
                }
                else
                {
                    AppLog_Printf("[FAN] ERROR use F0..F4\r\n");
                }
            }
            else if (command == 'F')
            {
                fan_prefix = true;
                fan_prefix_at = HAL_GetTick();
            }
            else if (command == 'A')
            {
                bench_servo_ch = SERVO_CH_OPEN;
                AppLog_Printf("[SERVO_BENCH] select ch=1 (PE9/D6 OPEN) deg=%u\r\n",
                              (unsigned)Servo_GetChannelAngleDeg(bench_servo_ch));
            }
            else if (command == 'B')
            {
                bench_servo_ch = SERVO_CH_CLOSE;
                AppLog_Printf("[SERVO_BENCH] select ch=2 (PE11/D5 CLOSE) deg=%u\r\n",
                              (unsigned)Servo_GetChannelAngleDeg(bench_servo_ch));
            }
            else if (command == 'S')
            {
                servo_digits_left = 3U;
                servo_acc = 0U;
                servo_prefix_at = HAL_GetTick();
            }
            else if (command == '+')
            {
                uint16_t cur = Servo_GetChannelAngleDeg(bench_servo_ch);
                uint16_t deg = (cur + 5U > 180U) ? 180U : (uint16_t)(cur + 5U);
                Servo_SetChannelAngleDeg(bench_servo_ch, deg);
                AppLog_Printf("[SERVO_BENCH] ch=%u deg=%u pulse_us=%u\r\n",
                              (unsigned)bench_servo_ch + 1U, (unsigned)deg,
                              (unsigned)Servo_GetChannelPulseUs(bench_servo_ch));
            }
            else if (command == '-')
            {
                uint16_t cur = Servo_GetChannelAngleDeg(bench_servo_ch);
                uint16_t deg = (cur < 5U) ? 0U : (uint16_t)(cur - 5U);
                Servo_SetChannelAngleDeg(bench_servo_ch, deg);
                AppLog_Printf("[SERVO_BENCH] ch=%u deg=%u pulse_us=%u\r\n",
                              (unsigned)bench_servo_ch + 1U, (unsigned)deg,
                              (unsigned)Servo_GetChannelPulseUs(bench_servo_ch));
            }
            else if (command == 'Z')
            {
                Servo_SetChannelAngleDeg(bench_servo_ch, 0U);
                AppLog_Printf("[SERVO_BENCH] ch=%u deg=0 pulse_us=%u\r\n",
                              (unsigned)bench_servo_ch + 1U,
                              (unsigned)Servo_GetChannelPulseUs(bench_servo_ch));
            }
            else if (command == 'M')
            {
                Servo_SetChannelAngleDeg(bench_servo_ch, 45U);
                AppLog_Printf("[SERVO_BENCH] ch=%u deg=45 pulse_us=%u\r\n",
                              (unsigned)bench_servo_ch + 1U,
                              (unsigned)Servo_GetChannelPulseUs(bench_servo_ch));
            }
            else if (command == 'O' || command == 'C')
            {
                const bool want_open = (command == 'O');
                const ServoChannel retreat_ch = want_open ? SERVO_CH_CLOSE : SERVO_CH_OPEN;
                Servo_SetChannelAngleDeg(retreat_ch, 0U);
                win_seq.target_open = want_open;
                win_seq.step = WIN_SEQ_RETREAT;
                win_seq.step_at = HAL_GetTick();
                AppLog_Printf("[WIN_BENCH] start %s: retreat ch=%u -> 0deg\r\n",
                              want_open ? "OPEN" : "CLOSE", (unsigned)retreat_ch + 1U);
            }
            else if (command == 'R')
            {
                AppReed_Snapshot reed;
                AppReed_GetSnapshot(&reed);
                AppLog_Printf("[REED_BENCH] query valid=%u stable_do=%u\r\n",
                              reed.valid ? 1U : 0U, reed.do_high ? 1U : 0U);
            }
            else if (command == 'V')
            {
                AppVision_Rehandshake();
            }
            else if (command >= '0' && command <= '4')
            {
                if(command=='0')AppControl_SafetyOff();
                else AppLog_Printf("[CTRL] REJECTED bench brightness: controller owns outputs; B1 short resumes AUTO\r\n");
            }
            else if (command != '\r' && command != '\n' && command != ' ' && command != '\t')
            {
                AppLog_Printf("[CTRL] ERROR byte=0x%02X; use B1, 0/F0, V, A/B, +/-, Z, M, S000..S180, O/C, R\r\n", command);
            }
        }
        uint32_t now=HAL_GetTick();
        if((uint32_t)(now-control_at)>=20U) {
            control_at=now;
            ControlOutput before;AppControl_GetSnapshot(&before);
            ControlOutput next=AppControl_Poll();
            /* Fan control exposes five PWM levels. The ring still uses its fixed ON brightness. */
            uint8_t fan_level=next.fan_level,led_level=next.led?2U:0U;
            bool fan_ok=AppFan_GetLevel()==fan_level || AppFan_SetLevel(fan_level);
            bool led_ok=AppLed_GetLevel()==led_level || show_level(led_level);
            if(!fan_ok || !led_ok) {
                AppLog_Printf("[CTRL] actuator write failed; safety OFF and reset\r\n");
                (void)AppFan_SetLevel(0U);(void)show_level(0U);
                NVIC_SystemReset();
            }
            AppControl_PublishApplied(&next);
            if(before.occupancy!=next.occupancy || before.mode!=next.mode ||
               before.control_epoch!=next.control_epoch || before.led!=next.led || before.fan_level!=next.fan_level) {
                static const char *names[]={"UNKNOWN","OCCUPIED","VACANT"};
                AppLog_Printf("[CTRL] mode=%s occupancy=%s fan=%u led=%u epoch=%lu stack_free=%lu\r\n",
                    Control_ModeName(next.mode),names[next.occupancy],
                    (unsigned)next.fan_level,next.led?1U:0U,(unsigned long)next.control_epoch,
                    (unsigned long)osThreadGetStackSpace(osThreadGetId()));
            }
            if(next.requires_new_boot) {
                AppLog_Printf("[CTRL] epoch exhausted; reboot for a new boot_id\r\n");
                NVIC_SystemReset();
            }
        }
        osDelay(1U);
    }
}
