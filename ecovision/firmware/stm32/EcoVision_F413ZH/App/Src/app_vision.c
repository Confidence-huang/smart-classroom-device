#include "app_vision.h"
#include "app_log.h"
#include "app_lcd.h"
#include "vision_protocol.h"
#include "cmsis_os.h"
#include <stdio.h>

enum { RING_SIZE=1024 };
typedef struct {uint32_t tick;uint8_t byte;} Sample;
static UART_HandleTypeDef *port;
static uint8_t rx_byte;
static volatile Sample ring[RING_SIZE];
static volatile uint16_t head, tail;
static volatile uint32_t faults;
static volatile bool restart;
static EvVision state;
static bool lcd_enabled;

void AppVision_GetSnapshot(EvVision *out) {
    taskENTER_CRITICAL();*out=state;taskEXIT_CRITICAL();
    if(!out->active || (uint32_t)(HAL_GetTick()-out->fresh_at)>=5000U)out->valid=false;
}
void AppVision_Rehandshake(void) {restart=true;}
static void arm(void) {
    if(HAL_UART_Receive_IT(port,&rx_byte,1)!=HAL_OK) faults|=0x10000U;
}
void AppVision_RxComplete(UART_HandleTypeDef *uart) {
    if(uart!=port) return;
    uint16_t next=(uint16_t)((head+1U)%RING_SIZE);
    if(next==tail) faults|=0x20000U;
    else {ring[head].tick=HAL_GetTick();ring[head].byte=rx_byte;head=next;}
    arm();
}
void AppVision_RxError(UART_HandleTypeDef *uart) {
    if(uart==port) faults|=uart->ErrorCode ? uart->ErrorCode : 0x40000U;
}
static bool random_word(uint32_t *value) {
    uint32_t start=HAL_GetTick();
    while(!(RNG->SR&RNG_SR_DRDY)) {
        if((RNG->SR&(RNG_SR_CECS|RNG_SR_SECS)) || HAL_GetTick()-start>=100U) return false;
    }
    if(RNG->SR&(RNG_SR_CECS|RNG_SR_SECS)) return false;
    *value=RNG->DR;return true;
}
static bool begin(uint32_t now) {
    uint32_t a,b;
    if(!random_word(&a)||!random_word(&b)) return false;
    char id[17];
    (void)snprintf(id,sizeof id,"%08lx%08lx",(unsigned long)a,(unsigned long)b);
    EvVision_Init(&state,id,now);
    if(lcd_enabled && !AppLcd_Show(false,false,0,0)) {
        lcd_enabled=false;AppLog_Printf("[LCD] SPI transfer failed\r\n");
    }
    AppLog_Printf("[VISION] WAIT link=%s (UART2 PD5 TX/PD6 RX, 115200 8N1)\r\n",id);
    return true;
}
void AppVision_Run(UART_HandleTypeDef *uart) {
    osDelay(1100U);
    port=uart;
    lcd_enabled=AppLcd_Init();
    AppLog_Printf("[LCD] ST7735 SPI1 PA5/PA7 DC=PF12 CS=PD14 init=%s\r\n",lcd_enabled?"OK":"FAILED");
    __HAL_RCC_RNG_CLK_ENABLE();
    RNG->CR=RNG_CR_RNGEN;
    if(!begin(HAL_GetTick())) {
        AppLog_Printf("[VISION] RNG failed; no session created\r\n");
        for(;;) osDelay(1000U);
    }
    arm();
    AppLog_Printf("[VISION] heap_free=%lu bytes; task_stack_free=%lu bytes\r\n",
        (unsigned long)xPortGetFreeHeapSize(),(unsigned long)osThreadGetStackSpace(osThreadGetId()));
    uint32_t query_at=HAL_GetTick()-1000U;
    uint32_t last_errors=0;
    uint32_t lcd_at=HAL_GetTick()-1000U;
    for(;;) {
        uint32_t now=HAL_GetTick();
        if(faults) {
            HAL_NVIC_DisableIRQ(USART2_IRQn);
            uint32_t error=faults;
            (void)HAL_UART_AbortReceive(port);
            tail=head;faults=0;
            HAL_NVIC_EnableIRQ(USART2_IRQn);
            arm();restart=true;
            AppLog_Printf("[VISION] RX fault=0x%lx; discard and rehandshake\r\n",(unsigned long)error);
        }
        if(restart || EvVision_Expired(&state,now)) {
            AppLog_Printf("[VISION] UNKNOWN timeout/restart; previous count invalid\r\n");
            restart=false;
            if(!begin(now)) {
                state.active=false;state.valid=false;
                AppLog_Printf("[VISION] RNG failed; UART session disabled\r\n");
                for(;;) osDelay(1000U);
            }
            query_at=now-1000U;last_errors=0;
        }
        while(tail!=head) {
            uint8_t byte=ring[tail].byte;
            uint32_t at=ring[tail].tick;
            tail=(uint16_t)((tail+1U)%RING_SIZE);
            bool was_active=state.active;
            uint32_t accepted=state.accepted;
            EvVision_Feed(&state,byte,at);
            if(!was_active && state.active) AppLog_Printf("[VISION] HELLO link=%s boot=%s\r\n",state.link,state.boot);
            if(state.accepted!=accepted) {
                AppLog_Printf("[VISION] RX seq=%lu count=%u valid=%u uptime=%lu link=%s boot=%s\r\n",
                    (unsigned long)state.seq,state.count,state.valid?1U:0U,(unsigned long)state.uptime,state.link,state.boot);
            }
        }
        if(state.errors!=last_errors) {
            last_errors=state.errors;
            AppLog_Printf("[VISION] bad_frames=%lu rejected=%lu\r\n",(unsigned long)state.errors,(unsigned long)state.rejected);
        }
        if(!state.active && now-query_at>=1000U) {
            char body[32],packet[48];
            int n=snprintf(body,sizeof body,"EVQ,%s",state.link);
            uint16_t crc=EvVision_Crc((const uint8_t *)body,(size_t)n);
            n=snprintf(packet,sizeof packet,"$%s*%04X\r\n",body,(unsigned)crc);
            HAL_StatusTypeDef result=HAL_UART_Transmit(port,(uint8_t *)packet,(uint16_t)n,25U);
            AppLog_Printf("[VISION] TX %s%s",result==HAL_OK?"":"FAILED ",packet);
            query_at=now;
        }
        if(lcd_enabled && (uint32_t)(HAL_GetTick()-lcd_at)>=1000U) {
            if(!AppLcd_Show(state.active,state.valid,state.count,state.seq)) {
                lcd_enabled=false;AppLog_Printf("[LCD] refresh failed; UART continues\r\n");
            }
            lcd_at=HAL_GetTick();
        }
        osDelay(2U);
    }
}
