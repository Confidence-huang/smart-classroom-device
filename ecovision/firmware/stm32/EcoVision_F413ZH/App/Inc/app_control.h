#ifndef APP_CONTROL_H
#define APP_CONTROL_H
#include "control_policy.h"
#ifdef ECOVISION_DAYLIGHT_SHADOW
void AppControl_LogDaylight(void);
#endif
void AppControl_Init(void);
ControlOutput AppControl_Poll(void);
void AppControl_PublishApplied(const ControlOutput *output);
void AppControl_GetSnapshot(ControlOutput *output);
void AppControl_SafetyOff(void);
#endif
