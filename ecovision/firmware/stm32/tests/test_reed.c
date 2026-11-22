#include "app_reed.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static uint32_t tick;
static GPIO_PinState pin_state = GPIO_PIN_SET;
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{ (void)port; (void)pin; return pin_state; }
uint32_t HAL_GetTick(void) { return tick; }
void AppLog_Printf(const char *format, ...) { (void)format; }
int main(void)
{
    AppReed_Filter f = {0};
    assert(!AppReed_Update(&f, true, 0U) && !f.valid);
    assert(!AppReed_Update(&f, true, 29U) && !f.valid);
    assert(AppReed_Update(&f, true, 30U) && f.valid && f.stable_high);
    assert(!AppReed_Update(&f, true, 40U));
    assert(!AppReed_Update(&f, false, 50U));
    assert(!AppReed_Update(&f, true, 60U));
    assert(!AppReed_Update(&f, false, 70U));
    assert(!AppReed_Update(&f, false, 99U) && f.stable_high);
    assert(AppReed_Update(&f, false, 100U) && !f.stable_high);
    assert(!AppReed_Update(&f, false, 200U));
    AppReed_Filter wrap = {0};
    assert(!AppReed_Update(&wrap, true, UINT32_MAX - 15U));
    assert(!AppReed_Update(&wrap, true, 13U));
    assert(AppReed_Update(&wrap, true, 14U) && wrap.valid);
    AppReed_Snapshot snapshot;
    AppReed_GetSnapshot(&snapshot);
    assert(!snapshot.valid);
    AppReed_GetSnapshot(NULL);
    AppReed_Poll();
    tick = 30U;
    AppReed_Poll();
    AppReed_GetSnapshot(&snapshot);
    assert(snapshot.valid && snapshot.do_high && snapshot.sampled_at == 30U);
    tick = 3029U;
    AppReed_GetSnapshot(&snapshot);
    assert(snapshot.valid);
    tick = 3030U;
    AppReed_GetSnapshot(&snapshot);
    assert(!snapshot.valid);
    pin_state = GPIO_PIN_RESET;
    AppReed_Poll();
    tick = 3060U;
    AppReed_Poll();
    AppReed_GetSnapshot(&snapshot);
    assert(snapshot.valid && !snapshot.do_high);
    puts("PASS: initial unknown, 30ms stability, bounce rejection, both levels, no duplicate events, tick wrap, snapshot expiry and recovery");
    return 0;
}
