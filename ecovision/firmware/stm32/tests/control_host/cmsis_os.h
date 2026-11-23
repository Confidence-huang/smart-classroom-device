#ifndef CONTROL_HOST_OS_H
#define CONTROL_HOST_OS_H
extern int critical_depth;
#define taskENTER_CRITICAL() (++critical_depth)
#define taskEXIT_CRITICAL() (--critical_depth)
#endif
