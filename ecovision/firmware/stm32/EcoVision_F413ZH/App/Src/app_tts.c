/* Producer tasks only enqueue lightweight voice events; networkTask owns UART6 AT+QTTS. */
#include "app_tts.h"
#include "cmsis_os.h"
#include "main.h"

static TtsPolicy tts_policy;
static uint32_t tts_init_at;

void AppTts_Init(bool watchdog_reset)
{
    taskENTER_CRITICAL();
    tts_init_at = HAL_GetTick();
    TtsPolicy_Init(&tts_policy, watchdog_reset);
    taskEXIT_CRITICAL();
}

void AppTts_SpeakAction(const TtsActionRequest *req, bool interrupt)
{
    if (!req)
    {
        return;
    }
    taskENTER_CRITICAL();
    TtsPolicy_PostAction(&tts_policy, req, interrupt);
    taskEXIT_CRITICAL();
}

void AppTts_SpeakAuto(TtsAutoType auto_event)
{
    const uint32_t now = HAL_GetTick();
    taskENTER_CRITICAL();
    TtsPolicy_PostAuto(&tts_policy, auto_event, now);
    taskEXIT_CRITICAL();
}

void AppTts_UpdateFaults(uint8_t raw_fault_mask)
{
    const uint32_t now = HAL_GetTick();
    /* Allow sensors and K230 to finish booting before triggering non-emergency fault alarms. */
    uint8_t filtered = raw_fault_mask;
    if ((uint32_t)(now - tts_init_at) < TTS_STARTUP_GRACE_MS)
    {
        filtered &= (uint8_t)TTS_FAULT_LOCAL_LOCK;
    }
    taskENTER_CRITICAL();
    TtsPolicy_UpdateFaults(&tts_policy, now, filtered);
    taskEXIT_CRITICAL();
}

bool AppTts_ToggleMute(void)
{
    bool muted = false;
    taskENTER_CRITICAL();
    muted = TtsPolicy_ToggleMute(&tts_policy);
    taskEXIT_CRITICAL();
    return muted;
}

bool AppTts_PollDispatch(bool modem_playing, TtsDispatch *out)
{
    if (!out)
    {
        return false;
    }
    const uint32_t now = HAL_GetTick();
    bool ready = false;
    taskENTER_CRITICAL();
    ready = TtsPolicy_Poll(&tts_policy, now, modem_playing, out);
    taskEXIT_CRITICAL();
    return ready;
}
