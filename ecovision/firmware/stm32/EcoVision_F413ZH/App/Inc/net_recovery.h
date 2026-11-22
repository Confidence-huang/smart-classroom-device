#ifndef NET_RECOVERY_H
#define NET_RECOVERY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool known; uint32_t baud; } NetAtLink;
typedef struct {
 void *context;
 int (*probe)(void *context,uint32_t baud);
 bool (*escape_legacy_ppp)(void *context);
 void (*configure)(void *context,uint32_t baud);
} NetRecoveryIo;
bool Net_FindAt(NetAtLink *link,const NetRecoveryIo *io);
#endif
