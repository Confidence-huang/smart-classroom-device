#ifndef CONTROL_POLICY_H
#define CONTROL_POLICY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { CONTROL_UNKNOWN, CONTROL_OCCUPIED, CONTROL_VACANT } ControlOccupancy;
typedef enum { CONTROL_AUTO, CONTROL_LOCAL_OFF, CONTROL_REMOTE } ControlMode;
typedef struct {
    bool vision_valid, new_vision_frame, new_vision_session;
    uint8_t count;
    uint32_t vision_age_ms;
    bool temperature_valid;
    int32_t temperature_x100;
    uint32_t temperature_age_ms;
    bool button_pressed;
    bool air_valid;
    uint16_t eco2_ppm;
    uint16_t tvoc_ppb;
    bool lux_valid;
    uint32_t lux_x100;
} ControlInput;
/* 0 stops the fan. AUTO and legacy remote ON use the proven level 2; high load boosts to 4. */
enum { CONTROL_FAN_OFF = 0, CONTROL_FAN_DEFAULT = 2, CONTROL_FAN_BOOST = 4, CONTROL_FAN_MAX = 5 };
typedef struct {
    ControlOccupancy occupancy;
    ControlMode mode;
    uint32_t control_epoch;
    bool led, requires_new_boot;
    uint8_t fan_level; /* Requested PWM level, carried through to the applied snapshot. */
    uint16_t hold_remaining_s; /* Rounded up while a remote command owns the outputs. */
} ControlOutput;
typedef struct {
    ControlOutput output;
    bool auto_led, auto_fan; /* AUTO keeps evaluating its own outputs during a remote lease. */
    bool auto_air_fan, auto_boost, auto_daylight_off;
    uint32_t remote_started_at, remote_duration_ms;
    uint8_t positives;
    bool zero_active, hot_active, cold_active, fault_active;
    bool air_bad_active, air_ok_active;
    bool daylight_bright_active, daylight_dark_active;
    uint32_t zero_at, hot_at, cold_at, fault_at;
    uint32_t air_bad_at, air_ok_at;
    uint32_t daylight_bright_at, daylight_dark_at;
    bool button_candidate, button_stable, long_handled;
    uint32_t button_changed_at, pressed_at;
} ControlPolicy;

void Control_Init(ControlPolicy *policy);
void Control_Update(ControlPolicy *policy, uint32_t now, const ControlInput *input);
void Control_LocalOff(ControlPolicy *policy);
void Control_BumpEpoch(ControlPolicy *policy);
/* Keep the AUTO fan and demo AC on the same temperature validity rules. */
bool Control_TemperatureFresh(const ControlInput *input);

/* hold_s == 0 requests AUTO. Only the task that reads B1 can apply a request. */
bool Control_Request(ControlPolicy *policy, uint32_t now, uint32_t epoch,
                     uint16_t hold_s, uint8_t fan_level, bool led);

const char *Control_ModeName(ControlMode mode);
#endif
