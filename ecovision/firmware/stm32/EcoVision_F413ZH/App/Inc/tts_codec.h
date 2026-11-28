#ifndef TTS_CODEC_H
#define TTS_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    TTS_FAULT_VISION     = 1U << 0,
    TTS_FAULT_SENSOR     = 1U << 1,
    TTS_FAULT_AIR_POOR   = 1U << 2,
    TTS_FAULT_LOCAL_LOCK = 1U << 3,
    TTS_FAULT_COUNT      = 4U
};

enum {
    TTS_FAULT_CONFIRM_MS = 10000U,
    TTS_FAULT_REPEAT_MS  = 18000U,
    TTS_AUTO_COOLDOWN_MS = 15000U,
    TTS_MIN_GAP_MS       = 1800U,
    TTS_INTERRUPT_GAP_MS = 350U,
    TTS_STARTUP_GRACE_MS = 15000U
};

typedef enum {
    TTS_ACT_NONE = 0,
    TTS_ACT_BOOT,
    TTS_ACT_WATCHDOG,
    TTS_ACT_MUTE_ON,
    TTS_ACT_MUTE_OFF,
    TTS_ACT_REMOTE_AUTO,
    TTS_ACT_REMOTE_FAN_LED,
    TTS_ACT_REMOTE_AC,
    TTS_ACT_IR_ALL_OFF,
    TTS_ACT_IR_AUTO,
    TTS_ACT_IR_LED_ON,
    TTS_ACT_IR_LED_OFF,
    TTS_ACT_IR_WHITE,
    TTS_ACT_IR_KTV,
    TTS_ACT_IR_RAINBOW,
    TTS_ACT_IR_WARM,
    TTS_ACT_IR_FAN,
    TTS_ACT_IR_AC,
    TTS_ACT_IR_LOCKED
} TtsActionType;

typedef enum {
    TTS_AUTO_NONE = 0,
    TTS_AUTO_VACANT_OFF,
    TTS_AUTO_DAYLIGHT_OFF,
    TTS_AUTO_AIR_VENT
} TtsAutoType;

typedef struct {
    TtsActionType type;
    uint8_t value;    /* fan_level (0..5) or ac_temp_c (0, 16..30) */
    uint8_t aux;      /* led (0/1) for REMOTE_FAN_LED */
    uint8_t prev_val; /* previous fan_level for REMOTE_FAN_LED diff */
    uint8_t prev_aux; /* previous led (0/1) for REMOTE_FAN_LED diff */
} TtsActionRequest;

typedef struct {
    bool muted;
    bool mute_farewell_pending;

    /* P0 Fault debounce & periodic repeat */
    uint8_t raw_faults;
    uint8_t active_faults;
    uint32_t fault_since[TTS_FAULT_COUNT];
    uint8_t fault_cursor;
    bool fault_immediate;
    uint32_t last_fault_spoken_at;
    bool fault_ever_spoken;
    bool recovery_pending;

    /* P1 User/Cloud immediate action (latest wins) */
    TtsActionRequest pending_action;
    bool action_interrupt;

    /* P2 Auto energy-saving hint (rate-limited) */
    TtsAutoType pending_auto;
    bool has_auto_spoken;
    uint32_t last_auto_at;

    /* Dispatch pacing */
    bool has_dispatched;
    uint32_t last_dispatch_at;
} TtsPolicy;

typedef struct {
    char command[256];
    char tag[32];
    bool interrupt;
} TtsDispatch;

/* Convert UTF-8 text (BMP U+0000..U+FFFF) to uppercase UCS2-BE hex string.
 * Returns hex length on success, or -1 on invalid UTF-8 or insufficient buffer. */
int Tts_Utf8ToUcs2Hex(const char *utf8, char *hex_out, size_t hex_cap);

/* Build EC200U AT+QTTS=1,"<UCS2-BE-HEX>" command from UTF-8 text.
 * Returns command length on success, or -1 on error. */
int Tts_FormatAtCommand(const char *utf8, char *cmd_out, size_t cmd_cap);

/* Format a Chinese phrase for an action into UTF-8 buffer (for testing/logging). */
int Tts_FormatActionUtf8(const TtsActionRequest *req, char *out, size_t cap);

/* Initialize policy state. Optionally queues boot greeting or watchdog warning. */
void TtsPolicy_Init(TtsPolicy *policy, bool watchdog_reset);

/* Queue a P1 immediate action (latest overwrites pending). */
void TtsPolicy_PostAction(TtsPolicy *policy, const TtsActionRequest *req, bool interrupt);

/* Queue a P2 automatic mode hint (subject to cooldown). */
void TtsPolicy_PostAuto(TtsPolicy *policy, TtsAutoType auto_event, uint32_t now);

/* Update raw fault bitmask; debounces sustained faults and schedules 18 s repeats. */
void TtsPolicy_UpdateFaults(TtsPolicy *policy, uint32_t now, uint8_t raw_fault_mask);

/* Toggle voice mute state. Returns new muted state (true = muted). */
bool TtsPolicy_ToggleMute(TtsPolicy *policy);

/* Poll for the next AT+QTTS command to transmit.
 * modem_playing: true if EC200U has not yet emitted +QTTS: 0 for the previous utterance.
 * Returns true and fills `out` when a command should be sent now. */
bool TtsPolicy_Poll(TtsPolicy *policy, uint32_t now, bool modem_playing, TtsDispatch *out);

#endif
