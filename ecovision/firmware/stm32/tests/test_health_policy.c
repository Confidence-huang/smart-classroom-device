#include "health_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 HealthPolicy h={0};
 assert(!Health_Ready(&h,0));
 for(unsigned i=0;i<HEALTH_COUNT;i++)Health_Beat(&h,i,100);
 assert(Health_Ready(&h,100));assert(Health_Ready(&h,1099));
 assert(!Health_Ready(&h,1100));
 Health_Beat(&h,HEALTH_CONTROL,1100);assert(!Health_Ready(&h,1100)); /* failure latched */
 h=(HealthPolicy){0};
 for(unsigned i=0;i<HEALTH_COUNT;i++)Health_Beat(&h,i,0xFFFFFFF0u);
 assert(Health_Ready(&h,20));
 h=(HealthPolicy){0};for(unsigned i=0;i<HEALTH_COUNT-1;i++)Health_Beat(&h,i,100);
 assert(!Health_Ready(&h,100));
 puts("PASS: watchdog all-seen startup, per-task deadline, stale latch, tick wrap");return 0;
}
