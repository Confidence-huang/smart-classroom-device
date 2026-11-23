#ifndef IR_PULSE_HOST_OS_H
#define IR_PULSE_HOST_OS_H
#include <stdint.h>
#include <stddef.h>
typedef void *osMessageQueueId_t;
enum {osOK=0,osErrorResource=-1};
osMessageQueueId_t osMessageQueueNew(uint32_t,uint32_t,const void *);
int osMessageQueuePut(osMessageQueueId_t,const void *,uint8_t,uint32_t);
int osMessageQueueGet(osMessageQueueId_t,void *,uint8_t *,uint32_t);
int osMessageQueueReset(osMessageQueueId_t);
void osDelay(uint32_t);
#endif
