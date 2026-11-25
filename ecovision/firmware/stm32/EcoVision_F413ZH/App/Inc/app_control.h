#ifndef APP_CONTROL_H
#define APP_CONTROL_H
#include "control_policy.h"

typedef enum {
    CONTROL_APPLIED, CONTROL_EXPIRED, CONTROL_EPOCH_MISMATCH, CONTROL_LOCAL_LOCKED,
    CONTROL_AC_ACCEPTED, CONTROL_AC_REJECTED, CONTROL_AC_UNKNOWN
} ControlResult;

/* Single command in flight. The output task reads B1 before it handles the command. */
typedef struct {
    uint32_t epoch;
    uint32_t expires_at; /* Local monotonic deadline, computed from the trusted clock. */
    uint16_t hold_s;
    uint8_t fan_level; /* Absolute setting from the validated network command. */
    bool led;
    bool ac_demo;
    uint8_t ac_temp_c; /* Absolute demo setting, 0 means off. */
    char id[65];
} ControlCommand;

typedef struct {
    ControlResult result;
    ControlOutput output; /* Driver-confirmed snapshot associated with this command. */
} ControlReply;

bool AppControl_Queue(const ControlCommand *command);
bool AppControl_TakeReply(ControlReply *reply);
void AppControl_GetRemoteSnapshot(ControlOutput *output, char last_command[65]);
void AppControl_Init(void);
ControlOutput AppControl_Poll(void);
void AppControl_PublishApplied(const ControlOutput *output);
void AppControl_GetSnapshot(ControlOutput *output);
void AppControl_SafetyOff(void);
#endif
