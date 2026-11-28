#ifndef SGP30_HOST_CMSIS_OS_H
#define SGP30_HOST_CMSIS_OS_H
#include <stdint.h>
#define osOK 0
int32_t osDelay(uint32_t ticks);
static inline int32_t osKernelLock(void) { return 0; }
static inline int32_t osKernelRestoreLock(int32_t lock) { return lock; }
#endif
