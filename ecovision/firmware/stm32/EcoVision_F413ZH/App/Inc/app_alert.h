#ifndef APP_ALERT_H
#define APP_ALERT_H
#include <stdbool.h>
#include <stdint.h>
enum { ALERT_VISION=1u, ALERT_CLOUD=2u, ALERT_AUTO=4u, ALERT_SYSTEM=8u };
enum { HEALTH_CONTROL, HEALTH_OUTPUT, HEALTH_VISION, HEALTH_ENVIRONMENT, HEALTH_NETWORK, HEALTH_COUNT };
void AppAlert_Init(void);
void AppAlert_Control(bool vision_valid, bool automatic, bool remote, uint8_t fan, uint8_t led);
unsigned AppAlert_Take(void);
void AppAlert_Retry(unsigned event);
void AppHealth_Start(void);
void AppHealth_Beat(unsigned task);
void AppHealth_Poll(void);
#endif
