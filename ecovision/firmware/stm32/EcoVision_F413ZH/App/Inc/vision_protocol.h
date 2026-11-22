#ifndef VISION_PROTOCOL_H
#define VISION_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef struct {
    char link[17], boot[17], buffer[129];
    uint16_t used;
    bool active, valid, have_seq;
    uint8_t count;
    uint32_t seq, uptime, fresh_at, start_at, accepted, errors, rejected;
} EvVision;
uint16_t EvVision_Crc(const uint8_t *data, size_t size);
void EvVision_Init(EvVision *v, const char *link, uint32_t now);
void EvVision_Feed(EvVision *v, uint8_t byte, uint32_t now);
bool EvVision_Expired(const EvVision *v, uint32_t now);
#endif
