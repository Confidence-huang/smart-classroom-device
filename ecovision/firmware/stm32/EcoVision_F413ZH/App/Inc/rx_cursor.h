#ifndef RX_CURSOR_H
#define RX_CURSOR_H
#include <stdint.h>
typedef struct {uint16_t position;} RxCursor;
typedef struct {uint16_t offset,length;} RxSpan;
/* Position is derived from current NDTR, not a potentially delayed HAL event Size.
 * HT/TC must remain enabled and IRQ latency must be less than half a buffer. */
int RxCursor_Advance(RxCursor *,uint16_t position,uint16_t capacity,RxSpan spans[2]);
#endif
