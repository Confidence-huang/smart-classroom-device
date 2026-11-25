#ifndef ECOVISION_CORE_H
#define ECOVISION_CORE_H
#include "ecovision_algorithm.h"

enum { EV_SCORE_TEMP, EV_SCORE_RH, EV_SCORE_AIR, EV_SCORE_LIGHT, EV_SCORE_COUNT };
enum { EV_SCORE_UNKNOWN = 255 };
enum { EV_DEVICE_LIGHT, EV_DEVICE_CURTAIN, EV_DEVICE_WINDOW, EV_DEVICE_AC,
       EV_DEVICE_FAN, EV_DEVICE_COUNT };
enum { EV_HOLD, EV_OFF, EV_ON, EV_INCREASE, EV_DECREASE, EV_OPEN, EV_CLOSE,
       EV_SHADE, EV_COOL, EV_STOP_COOLING };
enum { EV_LUX_UNKNOWN, EV_DIM, EV_LUX_OK, EV_BRIGHT };
enum { EV_AIR_UNKNOWN, EV_AIR_OK, EV_VENTILATE };
enum { EV_SHADE_UNKNOWN, EV_DAYLIGHT_ALLOWED, EV_SHADE_NEEDED };
enum { EV_DAYLIGHT_DISABLED, EV_DAYLIGHT_SHARED_SENSOR, EV_DAYLIGHT_ISOLATED_SENSOR };

typedef struct {
    EvInput base; /* AHT20 temp/RH, PC5 ADC, validated vision count and mode */
    EvSample lux_x100; /* BH1750 or calibrated estimate, never direct ADC */
    EvSample co2_ppm; /* validated by a warmed-up CO2 driver */
    uint8_t vision_session_changed; /* pulse when parser accepts a new session */
    uint8_t simulation; /* explicit offline opt-in; output is never for hardware */
    /* These are current, independently qualified external states. Zero means
     * absent/unknown. Drivers clear permission on stale feedback or interlock. */
    uint8_t ready_mask; /* bit EV_DEVICE_*: installed, allowed, idle, cooldown met */
    uint8_t outdoor_open_permitted;
    uint8_t window_closed_confirmed;
    uint8_t window_open_confirmed; /* independently confirmed ventilation position */
    uint8_t ac_off_confirmed;
    uint8_t ac_cooling_confirmed;
    uint8_t daylight; /* EV_SHADE_* or EV_DAYLIGHT_ALLOWED */
    uint8_t daylight_switching; /* optional binary lighting experiment */
} EvCoreInput;

typedef struct {
    EvState base;
    EvChannel lux, co2;
    uint32_t air_since, light_since, ac_since;
    uint8_t air_pending, light_pending, ac_pending;
    uint8_t vent_demand, vent_cleared, ac_demand, previous_mode, previous_simulation;
    uint32_t daylight_since;
    uint8_t daylight_pending, previous_daylight_switching, previous_led_actual;
} EvCoreState;

typedef struct {
    EvOutput base; /* existing desk prototype output, including its apply_mask */
    uint8_t score[EV_SCORE_COUNT], score_mask, coverage_pct;
    uint8_t weighted_score, total_score, measured_score; /* 0..100 or 255 */
    uint8_t people_valid, people_count;
    uint16_t impact_x100[EV_SCORE_COUNT]; /* count × (100-score), UINT16_MAX unknown */
    uint8_t air_need, light_need, ventilation_blocked, simulation;
    uint8_t advice[EV_DEVICE_COUNT]; /* advisory only; not actuator commands */
    uint8_t daylight_led; /* optional EV_HOLD/EV_ON/EV_OFF, independent of base */
} EvCoreOutput;

void ev_core_init(EvCoreState *state);
void ev_core_step(EvCoreState *state, const EvCoreInput *input, EvCoreOutput *output);
/* Observe lighting using occupancy/mode from the owning firmware policy. */
uint8_t ev_daylight_observe(EvCoreState *state,const EvSample *lux,uint32_t now,
                            uint8_t occupancy,uint8_t mode,uint8_t lit,uint8_t policy,
                            uint8_t force_off);
#endif
