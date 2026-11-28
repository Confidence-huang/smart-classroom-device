#include "tts_codec.h"

#include <stdio.h>
#include <string.h>

/* Pre-verified UTF-8 byte literals (pure ASCII source for cross-compiler safety). */
#define TTS_U8_BOOT            "\xE7\xBB\xBF\xE6\x99\xBA\xE5\xBE\xAE\xE8\xA7\x86\xE7\xB3\xBB\xE7\xBB\x9F\xE5\xB7\xB2\xE5\x90\xAF\xE5\x8A\xA8"
#define TTS_U8_WATCHDOG        "\xE8\xAD\xA6\xE5\x91\x8A\xEF\xBC\x8C\xE7\xB3\xBB\xE7\xBB\x9F\xE6\xA3\x80\xE6\xB5\x8B\xE5\x88\xB0\xE5\xBC\x82\xE5\xB8\xB8\xE5\xB9\xB6\xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE9\x87\x8D\xE5\x90\xAF\xE6\x81\xA2\xE5\xA4\x8D"
#define TTS_U8_MUTE_ON         "\xE8\xAF\xAD\xE9\x9F\xB3\xE6\x92\xAD\xE6\x8A\xA5\xE5\xB7\xB2\xE9\x9D\x99\xE9\x9F\xB3"
#define TTS_U8_MUTE_OFF        "\xE8\xAF\xAD\xE9\x9F\xB3\xE6\x92\xAD\xE6\x8A\xA5\xE5\xB7\xB2\xE6\x81\xA2\xE5\xA4\x8D"
#define TTS_U8_FAULT_VISION    "\xE8\xAD\xA6\xE5\x91\x8A\xEF\xBC\x8C\xE8\xA7\x86\xE8\xA7\x89\xE8\xAF\x86\xE5\x88\xAB\xE6\xA8\xA1\xE5\x9D\x97\xE7\xA6\xBB\xE7\xBA\xBF\xEF\xBC\x8C\xE8\xAF\xB7\xE6\xA3\x80\xE6\x9F\xA5"
#define TTS_U8_FAULT_SENSOR    "\xE8\xAD\xA6\xE5\x91\x8A\xEF\xBC\x8C\xE6\xB8\xA9\xE6\xB9\xBF\xE5\xBA\xA6\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE6\x95\x85\xE9\x9A\x9C\xEF\xBC\x8C\xE8\xAF\xB7\xE6\xA3\x80\xE6\x9F\xA5"
#define TTS_U8_FAULT_AIR       "\xE8\xAD\xA6\xE5\x91\x8A\xEF\xBC\x8C\xE5\xAE\xA4\xE5\x86\x85\xE7\xA9\xBA\xE6\xB0\x94\xE8\xB4\xA8\xE9\x87\x8F\xE8\xBE\x83\xE5\xB7\xAE\xEF\xBC\x8C\xE8\xAF\xB7\xE6\xB3\xA8\xE6\x84\x8F\xE9\x80\x9A\xE9\xA3\x8E"
#define TTS_U8_FAULT_LOCK      "\xE6\xB3\xA8\xE6\x84\x8F\xEF\xBC\x8C\xE7\xB3\xBB\xE7\xBB\x9F\xE5\xB7\xB2\xE8\xBF\x9B\xE5\x85\xA5\xE6\x9C\xAC\xE5\x9C\xB0\xE5\xBA\x94\xE6\x80\xA5\xE9\x94\x81\xE5\xAE\x9A\xE7\x8A\xB6\xE6\x80\x81"
#define TTS_U8_FAULT_RECOVERED "\xE7\xB3\xBB\xE7\xBB\x9F\xE6\x95\x85\xE9\x9A\x9C\xE5\xB7\xB2\xE6\x81\xA2\xE5\xA4\x8D\xE6\xAD\xA3\xE5\xB8\xB8"
#define TTS_U8_REMOTE_PREFIX   "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C"
#define TTS_U8_REMOTE_AUTO     "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C\xE5\xB7\xB2\xE6\x81\xA2\xE5\xA4\x8D\xE8\x87\xAA\xE5\x8A\xA8\xE8\x8A\x82\xE8\x83\xBD\xE6\xA8\xA1\xE5\xBC\x8F"
#define TTS_U8_REMOTE_ALL_OFF  "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD\xE9\xA3\x8E\xE6\x89\x87\xE5\x92\x8C\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_REMOTE_LED_ON   "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C\xE7\x81\xAF\xE5\x85\x89\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF"
#define TTS_U8_REMOTE_LED_OFF  "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C\xE7\x81\xAF\xE5\x85\x89\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD"
#define TTS_U8_REMOTE_FAN_OFF  "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C\xE9\xA3\x8E\xE6\x89\x87\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD"
#define TTS_U8_REMOTE_AC_OFF   "\xE6\x94\xB6\xE5\x88\xB0\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8C\x87\xE4\xBB\xA4\xEF\xBC\x8C\xE7\xA9\xBA\xE8\xB0\x83\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD"
#define TTS_U8_IR_ALL_OFF      "\xE9\x81\xA5\xE6\x8E\xA7\xE5\x99\xA8\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD\xE5\x85\xA8\xE9\x83\xA8\xE8\xAE\xBE\xE5\xA4\x87"
#define TTS_U8_IR_AUTO         "\xE5\xB7\xB2\xE6\x81\xA2\xE5\xA4\x8D\xE8\x87\xAA\xE5\x8A\xA8\xE8\x8A\x82\xE8\x83\xBD\xE6\xA8\xA1\xE5\xBC\x8F"
#define TTS_U8_IR_LED_ON       "\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF\xE6\x9A\x96\xE7\x99\xBD\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_IR_LED_OFF      "\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_IR_WHITE        "\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF\xE7\xBA\xAF\xE7\x99\xBD\xE7\x85\xA7\xE6\x98\x8E\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_IR_KTV          "\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF\xE7\x82\xAB\xE5\xBD\xA9\xE5\x8A\xA8\xE6\x84\x9F\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_IR_RAINBOW      "\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF\xE5\xBD\xA9\xE8\x89\xB2\xE6\xB5\x81\xE5\x8A\xA8\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_IR_WARM         "\xE5\xB7\xB2\xE5\x88\x87\xE6\x8D\xA2\xE5\x9B\x9E\xE6\x9A\x96\xE7\x99\xBD\xE7\x81\xAF\xE5\x85\x89"
#define TTS_U8_IR_FAN_OFF      "\xE9\xA3\x8E\xE6\x89\x87\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD"
#define TTS_U8_IR_AC_OFF       "\xE7\xA9\xBA\xE8\xB0\x83\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD"
#define TTS_U8_IR_LOCKED       "\xE5\xBD\x93\xE5\x89\x8D\xE5\xA4\x84\xE4\xBA\x8E\xE6\x9C\xAC\xE5\x9C\xB0\xE9\x94\x81\xE5\xAE\x9A\xE7\x8A\xB6\xE6\x80\x81\xEF\xBC\x8C\xE6\x93\x8D\xE4\xBD\x9C\xE5\xB7\xB2\xE6\x8B\x92\xE7\xBB\x9D"
#define TTS_U8_AUTO_VACANT     "\xE6\x95\x99\xE5\xAE\xA4\xE6\x97\xA0\xE4\xBA\xBA\xEF\xBC\x8C\xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE5\x85\xB3\xE9\x97\xAD\xE7\x81\xAF\xE5\x85\x89\xE5\x92\x8C\xE9\xA3\x8E\xE6\x89\x87"
#define TTS_U8_AUTO_DAYLIGHT   "\xE5\xAE\xA4\xE5\x86\x85\xE5\x85\x89\xE7\xBA\xBF\xE5\x85\x85\xE8\xB6\xB3\xEF\xBC\x8C\xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE5\x85\xB3\xE9\x97\xAD\xE7\x81\xAF\xE5\x85\x89\xE8\x8A\x82\xE8\x83\xBD"
#define TTS_U8_AUTO_AIR        "\xE6\xA3\x80\xE6\xB5\x8B\xE5\x88\xB0\xE5\xAE\xA4\xE5\x86\x85\xE7\xA9\xBA\xE6\xB0\x94\xE5\x81\x8F\xE9\x97\xB7\xEF\xBC\x8C\xE5\xB7\xB2\xE8\x87\xAA\xE5\x8A\xA8\xE5\xBC\x80\xE5\x90\xAF\xE9\x80\x9A\xE9\xA3\x8E"
#define TTS_U8_PART_FAN_TO     "\xE9\xA3\x8E\xE6\x89\x87\xE5\xB7\xB2\xE8\xB0\x83\xE8\x87\xB3"
#define TTS_U8_PART_DANG       "\xE6\xA1\xA3"
#define TTS_U8_PART_COMMA_LON  "\xEF\xBC\x8C\xE7\x81\xAF\xE5\x85\x89\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF"
#define TTS_U8_PART_COMMA_LOFF "\xEF\xBC\x8C\xE7\x81\xAF\xE5\x85\x89\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD"
#define TTS_U8_PART_AC_TO      "\xE7\xA9\xBA\xE8\xB0\x83\xE5\xB7\xB2\xE8\xAE\xBE\xE4\xB8\xBA"
#define TTS_U8_PART_DU         "\xE5\xBA\xA6"

static const char *const cn_digits[11] = {
    "\xE9\x9B\xB6", /* 0 */
    "\xE4\xB8\x80", /* 1 */
    "\xE4\xBA\x8C", /* 2 */
    "\xE4\xB8\x89", /* 3 */
    "\xE5\x9B\x9B", /* 4 */
    "\xE4\xBA\x94", /* 5 */
    "\xE5\x85\xAD", /* 6 */
    "\xE4\xB8\x83", /* 7 */
    "\xE5\x85\xAB", /* 8 */
    "\xE4\xB9\x9D", /* 9 */
    "\xE5\x8D\x81"  /* 10 */
};

static bool append_str(char *buf, size_t cap, size_t *pos, const char *s)
{
    const size_t len = strlen(s);
    if (*pos + len + 1U > cap)
    {
        return false;
    }
    memcpy(buf + *pos, s, len);
    *pos += len;
    buf[*pos] = '\0';
    return true;
}

static bool append_cn_number(char *buf, size_t cap, size_t *pos, uint8_t value)
{
    if (value <= 10U)
    {
        return append_str(buf, cap, pos, cn_digits[value]);
    }
    if (value < 20U)
    {
        return append_str(buf, cap, pos, cn_digits[10]) &&
               append_str(buf, cap, pos, cn_digits[value - 10U]);
    }
    if (value <= 99U)
    {
        const uint8_t tens = (uint8_t)(value / 10U);
        const uint8_t ones = (uint8_t)(value % 10U);
        if (!append_str(buf, cap, pos, cn_digits[tens]) ||
            !append_str(buf, cap, pos, cn_digits[10]))
        {
            return false;
        }
        return ones == 0U ? true : append_str(buf, cap, pos, cn_digits[ones]);
    }
    return false;
}

int Tts_Utf8ToUcs2Hex(const char *utf8, char *hex_out, size_t hex_cap)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    if (!utf8 || !hex_out || hex_cap == 0U)
    {
        return -1;
    }
    size_t out_len = 0U;
    const uint8_t *p = (const uint8_t *)utf8;
    while (*p != 0U)
    {
        uint32_t cp = 0U;
        if ((p[0] & 0x80U) == 0U)
        {
            cp = p[0];
            p += 1;
        }
        else if ((p[0] & 0xE0U) == 0xC0U)
        {
            if ((p[1] & 0xC0U) != 0x80U)
            {
                return -1;
            }
            cp = ((uint32_t)(p[0] & 0x1FU) << 6) | (uint32_t)(p[1] & 0x3FU);
            if (cp < 0x80U)
            {
                return -1;
            }
            p += 2;
        }
        else if ((p[0] & 0xF0U) == 0xE0U)
        {
            if ((p[1] & 0xC0U) != 0x80U || (p[2] & 0xC0U) != 0x80U)
            {
                return -1;
            }
            cp = ((uint32_t)(p[0] & 0x0FU) << 12) |
                 ((uint32_t)(p[1] & 0x3FU) << 6) |
                 (uint32_t)(p[2] & 0x3FU);
            if (cp < 0x800U || (cp >= 0xD800U && cp <= 0xDFFFU))
            {
                return -1;
            }
            p += 3;
        }
        else
        {
            return -1;
        }

        if (out_len + 5U > hex_cap)
        {
            return -1;
        }
        hex_out[out_len++] = hex_chars[(cp >> 12) & 0x0FU];
        hex_out[out_len++] = hex_chars[(cp >> 8) & 0x0FU];
        hex_out[out_len++] = hex_chars[(cp >> 4) & 0x0FU];
        hex_out[out_len++] = hex_chars[cp & 0x0FU];
    }
    hex_out[out_len] = '\0';
    return (int)out_len;
}

int Tts_FormatAtCommand(const char *utf8, char *cmd_out, size_t cmd_cap)
{
    static const char prefix[] = "AT+QTTS=1,\"";
    const size_t prefix_len = sizeof(prefix) - 1U;
    if (!utf8 || !cmd_out || cmd_cap <= prefix_len + 2U)
    {
        return -1;
    }
    memcpy(cmd_out, prefix, prefix_len);
    const int hex_len = Tts_Utf8ToUcs2Hex(utf8, cmd_out + prefix_len, cmd_cap - prefix_len - 1U);
    if (hex_len <= 0)
    {
        return -1;
    }
    const size_t end = prefix_len + (size_t)hex_len;
    if (end + 2U > cmd_cap)
    {
        return -1;
    }
    cmd_out[end] = '"';
    cmd_out[end + 1U] = '\0';
    return (int)(end + 1U);
}

int Tts_FormatActionUtf8(const TtsActionRequest *req, char *out, size_t cap)
{
    if (!req || !out || cap == 0U)
    {
        return -1;
    }
    size_t pos = 0U;
    out[0] = '\0';
    bool ok = false;

    switch (req->type)
    {
    case TTS_ACT_BOOT:
        ok = append_str(out, cap, &pos, TTS_U8_BOOT);
        break;
    case TTS_ACT_WATCHDOG:
        ok = append_str(out, cap, &pos, TTS_U8_WATCHDOG);
        break;
    case TTS_ACT_MUTE_ON:
        ok = append_str(out, cap, &pos, TTS_U8_MUTE_ON);
        break;
    case TTS_ACT_MUTE_OFF:
        ok = append_str(out, cap, &pos, TTS_U8_MUTE_OFF);
        break;
    case TTS_ACT_REMOTE_AUTO:
        ok = append_str(out, cap, &pos, TTS_U8_REMOTE_AUTO);
        break;
    case TTS_ACT_REMOTE_FAN_LED:
    {
        const uint8_t fan = req->value > 5U ? 5U : req->value;
        const bool led = req->aux != 0U;
        const bool fan_changed = fan != req->prev_val;
        const bool led_changed = (req->aux != 0U) != (req->prev_aux != 0U);
        if (fan == 0U && !led)
        {
            if (fan_changed && !led_changed)
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_FAN_OFF);
            }
            else if (!fan_changed && led_changed)
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_LED_OFF);
            }
            else
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_ALL_OFF);
            }
        }
        else if (!fan_changed && led_changed)
        {
            ok = append_str(out, cap, &pos, led ? TTS_U8_REMOTE_LED_ON : TTS_U8_REMOTE_LED_OFF);
        }
        else if (fan_changed && !led_changed)
        {
            if (fan == 0U)
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_FAN_OFF);
            }
            else
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_PREFIX) &&
                     append_str(out, cap, &pos, TTS_U8_PART_FAN_TO) &&
                     append_cn_number(out, cap, &pos, fan) &&
                     append_str(out, cap, &pos, TTS_U8_PART_DANG);
            }
        }
        else
        {
            if (fan == 0U)
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_FAN_OFF) &&
                     append_str(out, cap, &pos, led ? TTS_U8_PART_COMMA_LON : TTS_U8_PART_COMMA_LOFF);
            }
            else
            {
                ok = append_str(out, cap, &pos, TTS_U8_REMOTE_PREFIX) &&
                     append_str(out, cap, &pos, TTS_U8_PART_FAN_TO) &&
                     append_cn_number(out, cap, &pos, fan) &&
                     append_str(out, cap, &pos, TTS_U8_PART_DANG) &&
                     append_str(out, cap, &pos, led ? TTS_U8_PART_COMMA_LON : TTS_U8_PART_COMMA_LOFF);
            }
        }
        break;
    }
    case TTS_ACT_REMOTE_AC:
        if (req->value == 0U)
        {
            ok = append_str(out, cap, &pos, TTS_U8_REMOTE_AC_OFF);
        }
        else
        {
            ok = append_str(out, cap, &pos, TTS_U8_REMOTE_PREFIX) &&
                 append_str(out, cap, &pos, TTS_U8_PART_AC_TO) &&
                 append_cn_number(out, cap, &pos, req->value) &&
                 append_str(out, cap, &pos, TTS_U8_PART_DU);
        }
        break;
    case TTS_ACT_IR_ALL_OFF:
        ok = append_str(out, cap, &pos, TTS_U8_IR_ALL_OFF);
        break;
    case TTS_ACT_IR_AUTO:
        ok = append_str(out, cap, &pos, TTS_U8_IR_AUTO);
        break;
    case TTS_ACT_IR_LED_ON:
        ok = append_str(out, cap, &pos, TTS_U8_IR_LED_ON);
        break;
    case TTS_ACT_IR_LED_OFF:
        ok = append_str(out, cap, &pos, TTS_U8_IR_LED_OFF);
        break;
    case TTS_ACT_IR_WHITE:
        ok = append_str(out, cap, &pos, TTS_U8_IR_WHITE);
        break;
    case TTS_ACT_IR_KTV:
        ok = append_str(out, cap, &pos, TTS_U8_IR_KTV);
        break;
    case TTS_ACT_IR_RAINBOW:
        ok = append_str(out, cap, &pos, TTS_U8_IR_RAINBOW);
        break;
    case TTS_ACT_IR_WARM:
        ok = append_str(out, cap, &pos, TTS_U8_IR_WARM);
        break;
    case TTS_ACT_IR_FAN:
        if (req->value == 0U)
        {
            ok = append_str(out, cap, &pos, TTS_U8_IR_FAN_OFF);
        }
        else
        {
            const uint8_t fan = req->value > 5U ? 5U : req->value;
            ok = append_str(out, cap, &pos, TTS_U8_PART_FAN_TO) &&
                 append_cn_number(out, cap, &pos, fan) &&
                 append_str(out, cap, &pos, TTS_U8_PART_DANG);
        }
        break;
    case TTS_ACT_IR_AC:
        if (req->value == 0U)
        {
            ok = append_str(out, cap, &pos, TTS_U8_IR_AC_OFF);
        }
        else
        {
            ok = append_str(out, cap, &pos, TTS_U8_PART_AC_TO) &&
                 append_cn_number(out, cap, &pos, req->value) &&
                 append_str(out, cap, &pos, TTS_U8_PART_DU);
        }
        break;
    case TTS_ACT_IR_LOCKED:
        ok = append_str(out, cap, &pos, TTS_U8_IR_LOCKED);
        break;
    default:
        return -1;
    }
    return ok ? (int)pos : -1;
}

static const char *action_tag(TtsActionType type)
{
    switch (type)
    {
    case TTS_ACT_BOOT:           return "BOOT";
    case TTS_ACT_WATCHDOG:       return "WATCHDOG";
    case TTS_ACT_MUTE_ON:        return "MUTE_ON";
    case TTS_ACT_MUTE_OFF:       return "MUTE_OFF";
    case TTS_ACT_REMOTE_AUTO:    return "REMOTE_AUTO";
    case TTS_ACT_REMOTE_FAN_LED: return "REMOTE_FAN_LED";
    case TTS_ACT_REMOTE_AC:      return "REMOTE_AC";
    case TTS_ACT_IR_ALL_OFF:     return "IR_ALL_OFF";
    case TTS_ACT_IR_AUTO:        return "IR_AUTO";
    case TTS_ACT_IR_LED_ON:      return "IR_LED_ON";
    case TTS_ACT_IR_LED_OFF:     return "IR_LED_OFF";
    case TTS_ACT_IR_WHITE:       return "IR_WHITE";
    case TTS_ACT_IR_KTV:         return "IR_KTV";
    case TTS_ACT_IR_RAINBOW:     return "IR_RAINBOW";
    case TTS_ACT_IR_WARM:        return "IR_WARM";
    case TTS_ACT_IR_FAN:         return "IR_FAN";
    case TTS_ACT_IR_AC:          return "IR_AC";
    case TTS_ACT_IR_LOCKED:      return "IR_LOCKED";
    default:                     return "ACTION";
    }
}

static const char *fault_phrase(unsigned bit_index, const char **tag_out)
{
    switch (bit_index)
    {
    case 0U: *tag_out = "FAULT_VISION"; return TTS_U8_FAULT_VISION;
    case 1U: *tag_out = "FAULT_SENSOR"; return TTS_U8_FAULT_SENSOR;
    case 2U: *tag_out = "FAULT_AIR";    return TTS_U8_FAULT_AIR;
    case 3U: *tag_out = "FAULT_LOCK";   return TTS_U8_FAULT_LOCK;
    default: *tag_out = "FAULT";        return NULL;
    }
}

static const char *auto_phrase(TtsAutoType type, const char **tag_out)
{
    switch (type)
    {
    case TTS_AUTO_VACANT_OFF:   *tag_out = "AUTO_VACANT";   return TTS_U8_AUTO_VACANT;
    case TTS_AUTO_DAYLIGHT_OFF: *tag_out = "AUTO_DAYLIGHT"; return TTS_U8_AUTO_DAYLIGHT;
    case TTS_AUTO_AIR_VENT:     *tag_out = "AUTO_AIR";      return TTS_U8_AUTO_AIR;
    default:                    *tag_out = "AUTO";          return NULL;
    }
}

void TtsPolicy_Init(TtsPolicy *policy, bool watchdog_reset)
{
    memset(policy, 0, sizeof(*policy));
    policy->pending_action.type = watchdog_reset ? TTS_ACT_WATCHDOG : TTS_ACT_BOOT;
    policy->action_interrupt = watchdog_reset;
}

void TtsPolicy_PostAction(TtsPolicy *policy, const TtsActionRequest *req, bool interrupt)
{
    if (!policy || !req || req->type == TTS_ACT_NONE)
    {
        return;
    }
    policy->pending_action = *req;
    policy->action_interrupt = interrupt;
}

void TtsPolicy_PostAuto(TtsPolicy *policy, TtsAutoType auto_event, uint32_t now)
{
    if (!policy || auto_event == TTS_AUTO_NONE)
    {
        return;
    }
    if (policy->has_auto_spoken && (uint32_t)(now - policy->last_auto_at) < TTS_AUTO_COOLDOWN_MS)
    {
        return;
    }
    policy->pending_auto = auto_event;
}

void TtsPolicy_UpdateFaults(TtsPolicy *policy, uint32_t now, uint8_t raw_fault_mask)
{
    if (!policy)
    {
        return;
    }
    const uint8_t prev_active = policy->active_faults;
    uint8_t next_active = 0U;

    for (unsigned i = 0U; i < TTS_FAULT_COUNT; ++i)
    {
        const uint8_t bit = (uint8_t)(1U << i);
        if ((raw_fault_mask & bit) != 0U)
        {
            if ((policy->raw_faults & bit) == 0U)
            {
                policy->fault_since[i] = now;
            }
            const uint32_t confirm_ms = (bit == TTS_FAULT_LOCAL_LOCK) ? 0U : TTS_FAULT_CONFIRM_MS;
            if ((uint32_t)(now - policy->fault_since[i]) >= confirm_ms)
            {
                next_active |= bit;
            }
        }
        else
        {
            policy->fault_since[i] = 0U;
        }
    }

    policy->raw_faults = raw_fault_mask;
    policy->active_faults = next_active;

    /* Newly activated fault triggers an immediate warning. */
    if ((next_active & (uint8_t)(~prev_active)) != 0U)
    {
        policy->fault_immediate = true;
        policy->recovery_pending = false;
    }
    /* All faults cleared after at least one warning was announced -> speak recovery once. */
    if (prev_active != 0U && next_active == 0U)
    {
        policy->fault_immediate = false;
        if (policy->fault_ever_spoken)
        {
            policy->recovery_pending = true;
            policy->fault_ever_spoken = false;
        }
    }
}

bool TtsPolicy_ToggleMute(TtsPolicy *policy)
{
    if (!policy)
    {
        return false;
    }
    policy->muted = !policy->muted;
    if (policy->muted)
    {
        /* Drop queued chatter and announce mute once before silencing. */
        policy->pending_action.type = TTS_ACT_NONE;
        policy->pending_auto = TTS_AUTO_NONE;
        policy->recovery_pending = false;
        policy->mute_farewell_pending = true;
    }
    else
    {
        policy->mute_farewell_pending = false;
        policy->fault_immediate = false;
        policy->pending_action = (TtsActionRequest){.type = TTS_ACT_MUTE_OFF};
        policy->action_interrupt = true;
    }
    return policy->muted;
}

static bool fill_dispatch(TtsPolicy *policy, uint32_t now, const char *utf8,
                          const char *tag, bool interrupt, TtsDispatch *out)
{
    if (!utf8 || Tts_FormatAtCommand(utf8, out->command, sizeof(out->command)) < 0)
    {
        return false;
    }
    (void)snprintf(out->tag, sizeof(out->tag), "%s", tag);
    out->interrupt = interrupt;
    policy->has_dispatched = true;
    policy->last_dispatch_at = now;
    return true;
}

bool TtsPolicy_Poll(TtsPolicy *policy, uint32_t now, bool modem_playing, TtsDispatch *out)
{
    if (!policy || !out)
    {
        return false;
    }

    /* Special case: single goodbye phrase when user presses TEST to mute. */
    if (policy->mute_farewell_pending)
    {
        if (policy->has_dispatched && (uint32_t)(now - policy->last_dispatch_at) < TTS_INTERRUPT_GAP_MS)
        {
            return false;
        }
        policy->mute_farewell_pending = false;
        return fill_dispatch(policy, now, TTS_U8_MUTE_ON, "MUTE_ON", true, out);
    }

    if (policy->muted)
    {
        policy->pending_action.type = TTS_ACT_NONE;
        policy->pending_auto = TTS_AUTO_NONE;
        policy->recovery_pending = false;
        return false;
    }

    const uint32_t since_last = policy->has_dispatched ? (uint32_t)(now - policy->last_dispatch_at) : 0xFFFFFFFFU;

    /* P1: Immediate user/cloud command feedback takes instant precedence over periodic repeats
     * unless a brand-new fault just fired. */
    const bool new_fault_now = (policy->active_faults != 0U) && policy->fault_immediate;

    /* P0 (new fault) or P0 (periodic repeat every 18 s) */
    const bool repeat_fault_due = (policy->active_faults != 0U) &&
        (!policy->fault_ever_spoken || (uint32_t)(now - policy->last_fault_spoken_at) >= TTS_FAULT_REPEAT_MS);

    if (new_fault_now)
    {
        if (since_last < TTS_INTERRUPT_GAP_MS)
        {
            return false;
        }
        for (unsigned k = 0U; k < TTS_FAULT_COUNT; ++k)
        {
            const unsigned idx = (policy->fault_cursor + k) % TTS_FAULT_COUNT;
            if ((policy->active_faults & (1U << idx)) != 0U)
            {
                const char *tag = NULL;
                const char *utf8 = fault_phrase(idx, &tag);
                policy->fault_cursor = (uint8_t)((idx + 1U) % TTS_FAULT_COUNT);
                policy->fault_immediate = false;
                policy->fault_ever_spoken = true;
                policy->last_fault_spoken_at = now;
                return fill_dispatch(policy, now, utf8, tag, true, out);
            }
        }
    }

    /* P1: Cloud / IR remote / Boot / Watchdog action */
    if (policy->pending_action.type != TTS_ACT_NONE)
    {
        const bool want_interrupt = policy->action_interrupt;
        if (want_interrupt)
        {
            if (since_last < TTS_INTERRUPT_GAP_MS)
            {
                return false;
            }
        }
        else if (modem_playing || since_last < TTS_MIN_GAP_MS)
        {
            return false;
        }
        char utf8[128];
        if (Tts_FormatActionUtf8(&policy->pending_action, utf8, sizeof(utf8)) > 0)
        {
            const char *tag = action_tag(policy->pending_action.type);
            policy->pending_action.type = TTS_ACT_NONE;
            policy->action_interrupt = false;
            return fill_dispatch(policy, now, utf8, tag, want_interrupt, out);
        }
        policy->pending_action.type = TTS_ACT_NONE;
        policy->action_interrupt = false;
    }

    /* P0: Periodic repeated fault warning (waits for current utterance to finish cleanly) */
    if (repeat_fault_due && !modem_playing && since_last >= TTS_MIN_GAP_MS)
    {
        for (unsigned k = 0U; k < TTS_FAULT_COUNT; ++k)
        {
            const unsigned idx = (policy->fault_cursor + k) % TTS_FAULT_COUNT;
            if ((policy->active_faults & (1U << idx)) != 0U)
            {
                const char *tag = NULL;
                const char *utf8 = fault_phrase(idx, &tag);
                policy->fault_cursor = (uint8_t)((idx + 1U) % TTS_FAULT_COUNT);
                policy->fault_ever_spoken = true;
                policy->last_fault_spoken_at = now;
                return fill_dispatch(policy, now, utf8, tag, false, out);
            }
        }
    }

    /* P0 recovery announcement when all faults have cleared */
    if (policy->recovery_pending && !modem_playing && since_last >= TTS_MIN_GAP_MS)
    {
        policy->recovery_pending = false;
        return fill_dispatch(policy, now, TTS_U8_FAULT_RECOVERED, "FAULT_RECOVERED", false, out);
    }

    /* P2: Automatic energy-saving status hint */
    if (policy->pending_auto != TTS_AUTO_NONE && !modem_playing && since_last >= TTS_MIN_GAP_MS)
    {
        const char *tag = NULL;
        const char *utf8 = auto_phrase(policy->pending_auto, &tag);
        policy->pending_auto = TTS_AUTO_NONE;
        policy->has_auto_spoken = true;
        policy->last_auto_at = now;
        return fill_dispatch(policy, now, utf8, tag, false, out);
    }

    return false;
}
