#ifndef CONTROL_POLICY_H
#define CONTROL_POLICY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { CONTROL_UNKNOWN, CONTROL_OCCUPIED, CONTROL_VACANT } ControlOccupancy;
typedef enum { CONTROL_AUTO, CONTROL_LOCAL_OFF } ControlMode;
typedef struct {
    bool vision_valid, new_vision_frame, new_vision_session;
    uint8_t count;
    uint32_t vision_age_ms;
    bool temperature_valid;
    int32_t temperature_x100;
    uint32_t temperature_age_ms;
    bool button_pressed;
} ControlInput;
typedef struct {
    ControlOccupancy occupancy;
    ControlMode mode;
    uint32_t control_epoch;
    bool led, fan, requires_new_boot;
} ControlOutput;
typedef struct {
    ControlOutput output;
    uint8_t positives;
    bool zero_active, hot_active, cold_active, fault_active;
    uint32_t zero_at, hot_at, cold_at, fault_at;
    bool button_candidate, button_stable, long_handled;
    uint32_t button_changed_at, pressed_at;
} ControlPolicy;

void Control_Init(ControlPolicy *policy);
void Control_Update(ControlPolicy *policy, uint32_t now, const ControlInput *input);
void Control_LocalOff(ControlPolicy *policy);
#endif
