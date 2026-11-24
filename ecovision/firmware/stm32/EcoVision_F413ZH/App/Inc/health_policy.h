#ifndef HEALTH_POLICY_H
#define HEALTH_POLICY_H
#include <stdbool.h>
#include <stdint.h>
enum {HEALTH_CONTROL,HEALTH_VISION,HEALTH_NET,HEALTH_ENV,HEALTH_IR,HEALTH_COUNT};
typedef struct {uint32_t at[HEALTH_COUNT],seen;bool failed;} HealthPolicy;
void Health_Beat(HealthPolicy *,unsigned,uint32_t);
bool Health_Ready(HealthPolicy *,uint32_t);
#endif
