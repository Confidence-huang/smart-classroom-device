#ifndef ECOVISION_ALGORITHM_H
#define ECOVISION_ALGORITHM_H
#include <stdint.h>

/* Internal integration API, NOT a new wire protocol. Values are integers:
 * temperature 0.01 C, RH 0.01 %RH, light read_u16 raw, vision integer count.
 * All timestamps are STM32 LOCAL uint32 milliseconds, never K230/UTC time.
 * Call at least every 100 ms; all intervals and gaps must be < 2^31 ms.
 * Single owner task; do not call concurrently from ISR/network tasks. */
enum { EV_TEMP, EV_RH, EV_LIGHT, EV_VISION, EV_CHANNELS };
enum { EV_UNKNOWN, EV_OCCUPIED, EV_VACANT };
enum { EV_AUTO, EV_REMOTE, EV_LOCAL_OFF };
enum { EV_MISSING=1, EV_DRIVER=2, EV_RANGE=4, EV_TIME=8,
       EV_STALE=16, EV_JUMP=32, EV_DUPLICATE=64 };
enum { EV_NEED_UNKNOWN, EV_COLD, EV_SUITABLE, EV_HOT };
enum { EV_RH_UNKNOWN, EV_DRY, EV_RH_SUITABLE, EV_WET };
enum { EV_REASON_OCCUPIED=1, EV_REASON_VACANT=2, EV_REASON_TEMP=4,
       EV_REASON_FAULT=8, EV_REASON_MANUAL=16, EV_REASON_LOCAL=32,
       EV_REASON_LIGHT_UNCALIBRATED=64, EV_REASON_OCCUPANCY_UNKNOWN=128,
       EV_REASON_TEMP_UNKNOWN=256 };

typedef struct {
    uint32_t at_ms;
    int32_t value;
    uint8_t present, valid, source, simulated;
    uint16_t driver_reason; /* driver-specific diagnostic, zero on success */
} EvSample;
typedef struct {
    EvSample last_attempt; /* latest offered raw sample, including failures */
    uint32_t at_ms, seen_ms, age_ms;
    int32_t raw, filtered, history[3];
    uint16_t reason, driver_reason;
    uint8_t has_value, has_seen, valid, stable, used, next, source, simulated;
} EvChannel;
typedef struct {
    uint32_t now_ms;
    EvSample sample[EV_CHANNELS];
    uint8_t mode, fan_actual, led_actual; /* already arbitrated by STM32 */
} EvInput;
typedef struct {
    /* Prototype descriptive boundaries only; NOT PMV or public AUTO limits. */
    int32_t cold_c100, hot_c100, dry_rh100, wet_rh100;
} EvComfortConfig;
typedef struct {
    EvChannel channel[EV_CHANNELS];
    EvComfortConfig comfort;
    uint32_t zero_since, dwell_since, fault_since;
    uint8_t occupancy, positive, zero_active, dwell_active, dwell_target;
    uint8_t fault_active, previous_mode;
} EvState;
typedef struct {
    uint8_t occupancy, thermal, light_need, humidity_need;
    uint8_t fan, led, apply_mask; /* bit 0 fan, bit 1 LED; 0 yields control */
    uint8_t vision_valid, temp_valid, degraded_mask, simulated_mask;
    uint8_t affected_mask; /* bit 0 fan, bit 1 LED: unavailable AUTO perception */
    uint16_t reasons;
    /* light_need remains UNKNOWN until calibrated; no servo/IR/PWM output. */
} EvOutput;
typedef struct {
    uint32_t since;
    uint8_t initialized, candidate, stable, known;
} EvDebounce;
void ev_init(EvState *s);
/* Call once on each newly accepted UART session before its first EV1 sample. */
void ev_reset_vision(EvState *s);
/* Ingest ordered, validated transport frames before a single control tick. */
void ev_ingest_vision(EvState *s,const EvSample *sample,uint32_t now);
void ev_step(EvState *s, const EvInput *in, EvOutput *out);
/* Return -1 until first stable level, then 0/1. No press/lease logic here. */
int ev_debounce(EvDebounce *s, uint32_t now, uint8_t level, uint32_t delay_ms);
#endif
