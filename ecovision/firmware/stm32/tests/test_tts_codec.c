#include "tts_codec.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_ucs2_hardware_vector(void)
{
    /* Exact UTF-8 bytes of the proven EC200U speaker bench phrase (2026-11-24-speaker-tts.log). */
    static const char phrase[] =
        "\xE7\xBB\xBF\xE6\x99\xBA\xE5\xBE\xAE\xE8\xA7\x86\xEF\xBC\x8C"
        "\xE8\xAF\xAD\xE9\x9F\xB3\xE6\xB5\x8B\xE8\xAF\x95";
    char hex[128];
    char cmd[160];

    int n = Tts_Utf8ToUcs2Hex(phrase, hex, sizeof(hex));
    assert(n == 36);
    assert(strcmp(hex, "7EFF667A5FAE89C6FF0C8BED97F36D4B8BD5") == 0);

    int m = Tts_FormatAtCommand(phrase, cmd, sizeof(cmd));
    assert(m > 0);
    assert(strcmp(cmd, "AT+QTTS=1,\"7EFF667A5FAE89C6FF0C8BED97F36D4B8BD5\"") == 0);

    /* Reject malformed UTF-8 and too-small output buffer. */
    assert(Tts_Utf8ToUcs2Hex("\xFF", hex, sizeof(hex)) < 0);
    assert(Tts_Utf8ToUcs2Hex(phrase, hex, 10U) < 0);
    assert(Tts_FormatAtCommand("", cmd, sizeof(cmd)) < 0);
}

static void test_dynamic_phrases_and_ir_scenes(void)
{
    char utf8[128];
    char cmd[256];

    /* IR key 9: rainbow colorful flowing light */
    TtsActionRequest r = {.type = TTS_ACT_IR_RAINBOW};
    assert(Tts_FormatActionUtf8(&r, utf8, sizeof(utf8)) > 0);
    assert(Tts_FormatAtCommand(utf8, cmd, sizeof(cmd)) > 0);
    assert(strcmp(cmd, "AT+QTTS=1,\"5DF25F00542F5F6982726D4152A8706F5149\"") == 0);

    /* Remote AC set to 26 C and OFF */
    TtsActionRequest ac_on = {.type = TTS_ACT_REMOTE_AC, .value = 26U};
    assert(Tts_FormatActionUtf8(&ac_on, utf8, sizeof(utf8)) > 0);
    assert(Tts_FormatAtCommand(utf8, cmd, sizeof(cmd)) > 0);

    TtsActionRequest ac_off = {.type = TTS_ACT_REMOTE_AC, .value = 0U};
    assert(Tts_FormatActionUtf8(&ac_off, utf8, sizeof(utf8)) > 0);
    assert(Tts_FormatAtCommand(utf8, cmd, sizeof(cmd)) > 0);

    /* Remote fan+LED diff phrasing: LED only, fan only, both, all off */
    TtsActionRequest led_only = {.type = TTS_ACT_REMOTE_FAN_LED, .value = 2U, .aux = 1U, .prev_val = 2U, .prev_aux = 0U};
    assert(Tts_FormatActionUtf8(&led_only, utf8, sizeof(utf8)) > 0);

    TtsActionRequest fan_only = {.type = TTS_ACT_REMOTE_FAN_LED, .value = 4U, .aux = 1U, .prev_val = 2U, .prev_aux = 1U};
    assert(Tts_FormatActionUtf8(&fan_only, utf8, sizeof(utf8)) > 0);

    TtsActionRequest all_off = {.type = TTS_ACT_REMOTE_FAN_LED, .value = 0U, .aux = 0U, .prev_val = 4U, .prev_aux = 1U};
    assert(Tts_FormatActionUtf8(&all_off, utf8, sizeof(utf8)) > 0);
}

static void test_fault_debounce_repeat_round_robin_and_recovery(void)
{
    TtsPolicy p;
    TtsDispatch d;
    TtsPolicy_Init(&p, false);

    /* Consume initial boot greeting. */
    assert(TtsPolicy_Poll(&p, 2000U, false, &d));
    assert(strcmp(d.tag, "BOOT") == 0);
    assert(!d.interrupt);

    /* Transient 5 s vision glitch must NOT trigger alarm. */
    TtsPolicy_UpdateFaults(&p, 5000U, TTS_FAULT_VISION);
    TtsPolicy_UpdateFaults(&p, 10000U, TTS_FAULT_VISION);
    assert(!TtsPolicy_Poll(&p, 10000U, false, &d));
    TtsPolicy_UpdateFaults(&p, 11000U, 0U);
    assert(!TtsPolicy_Poll(&p, 11000U, false, &d));

    /* Sustained 10 s vision + sensor fault triggers immediate P0 warning and cycles every 18 s. */
    TtsPolicy_UpdateFaults(&p, 20000U, TTS_FAULT_VISION | TTS_FAULT_SENSOR);
    TtsPolicy_UpdateFaults(&p, 29999U, TTS_FAULT_VISION | TTS_FAULT_SENSOR);
    assert(!TtsPolicy_Poll(&p, 29999U, false, &d));

    TtsPolicy_UpdateFaults(&p, 30000U, TTS_FAULT_VISION | TTS_FAULT_SENSOR);
    assert(TtsPolicy_Poll(&p, 30000U, true, &d)); /* New fault interrupts ongoing audio. */
    assert(strcmp(d.tag, "FAULT_VISION") == 0);
    assert(d.interrupt);

    /* Must not repeat before 18 s elapsed. */
    assert(!TtsPolicy_Poll(&p, 47999U, false, &d));
    /* At +18 s, round-robins to FAULT_SENSOR (does not interrupt active speech). */
    assert(!TtsPolicy_Poll(&p, 48000U, true, &d));
    assert(TtsPolicy_Poll(&p, 48000U, false, &d));
    assert(strcmp(d.tag, "FAULT_SENSOR") == 0);
    assert(!d.interrupt);

    /* At +36 s, cycles back to FAULT_VISION. */
    assert(TtsPolicy_Poll(&p, 66000U, false, &d));
    assert(strcmp(d.tag, "FAULT_VISION") == 0);

    /* Clearing all faults announces recovery once. */
    TtsPolicy_UpdateFaults(&p, 70000U, 0U);
    assert(TtsPolicy_Poll(&p, 70000U, false, &d));
    assert(strcmp(d.tag, "FAULT_RECOVERED") == 0);
    assert(!TtsPolicy_Poll(&p, 95000U, false, &d));
}

static void test_action_overwrite_auto_cooldown_and_mute(void)
{
    TtsPolicy p;
    TtsDispatch d;
    TtsPolicy_Init(&p, true);

    /* Watchdog reset startup warning fires with interrupt=true. */
    assert(TtsPolicy_Poll(&p, 1000U, false, &d));
    assert(strcmp(d.tag, "WATCHDOG") == 0 && d.interrupt);

    /* Rapid IR presses overwrite stale action; only latest speaks after min interrupt gap. */
    TtsActionRequest k7 = {.type = TTS_ACT_IR_WHITE};
    TtsActionRequest k8 = {.type = TTS_ACT_IR_KTV};
    TtsPolicy_PostAction(&p, &k7, true);
    TtsPolicy_PostAction(&p, &k8, true);
    assert(!TtsPolicy_Poll(&p, 1200U, true, &d));
    assert(TtsPolicy_Poll(&p, 1400U, true, &d));
    assert(strcmp(d.tag, "IR_KTV") == 0 && d.interrupt);

    /* P2 auto hint respects 15 s cooldown and waits for idle modem. */
    TtsPolicy_PostAuto(&p, TTS_AUTO_DAYLIGHT_OFF, 5000U);
    assert(!TtsPolicy_Poll(&p, 5000U, true, &d));
    assert(TtsPolicy_Poll(&p, 5000U, false, &d));
    assert(strcmp(d.tag, "AUTO_DAYLIGHT") == 0 && !d.interrupt);
    TtsPolicy_PostAuto(&p, TTS_AUTO_VACANT_OFF, 10000U);
    assert(!TtsPolicy_Poll(&p, 10000U, false, &d));
    TtsPolicy_PostAuto(&p, TTS_AUTO_VACANT_OFF, 21000U);
    assert(TtsPolicy_Poll(&p, 21000U, false, &d));
    assert(strcmp(d.tag, "AUTO_VACANT") == 0);

    /* TEST key mute: announces mute once, then silences actions and faults until unmuted. */
    assert(TtsPolicy_ToggleMute(&p) == true);
    assert(TtsPolicy_Poll(&p, 22000U, true, &d));
    assert(strcmp(d.tag, "MUTE_ON") == 0 && d.interrupt);

    TtsPolicy_PostAction(&p, &k7, true);
    TtsPolicy_UpdateFaults(&p, 25000U, TTS_FAULT_LOCAL_LOCK);
    assert(!TtsPolicy_Poll(&p, 26000U, false, &d));

    /* Unmute announces recovery and resumes fault warnings. */
    assert(TtsPolicy_ToggleMute(&p) == false);
    assert(TtsPolicy_Poll(&p, 27000U, false, &d));
    assert(strcmp(d.tag, "MUTE_OFF") == 0 && d.interrupt);
    assert(TtsPolicy_Poll(&p, 30000U, false, &d));
    assert(strcmp(d.tag, "FAULT_LOCK") == 0);
}

int main(void)
{
    test_ucs2_hardware_vector();
    test_dynamic_phrases_and_ir_scenes();
    test_fault_debounce_repeat_round_robin_and_recovery();
    test_action_overwrite_auto_cooldown_and_mute();
    puts("PASS: TTS UCS2-BE hardware vector, dynamic phrases, 18s fault repeat/round-robin, recovery, and mute toggle");
    return 0;
}
