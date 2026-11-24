#include "health_policy.h"
void Health_Beat(HealthPolicy *h,unsigned id,uint32_t now) {
 if(id<HEALTH_COUNT){h->at[id]=now;h->seen|=1UL<<id;}
}
bool Health_Ready(HealthPolicy *h,uint32_t now) {
 static const uint32_t limits[HEALTH_COUNT]={1000,3000,5000,5000,2000};
 if(h->failed || h->seen!=((1UL<<HEALTH_COUNT)-1U))return false;
 for(unsigned i=0;i<HEALTH_COUNT;i++)if((uint32_t)(now-h->at[i])>=limits[i]){h->failed=true;return false;}
 return true;
}
