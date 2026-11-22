#ifndef NET_PUBLISH_H
#define NET_PUBLISH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *context;
    int (*begin)(void *context,const char *topic,uint16_t id,bool retain,size_t length);
    bool (*write_payload)(void *context,const char *payload,size_t length);
    bool (*wait_ack)(void *context,uint16_t id);
} NetPublishIo;
/* begin returns 0 for prompt, -2 for uncertain/missing prompt, other = reject.
 * For an uncertain prompt complete the declared payload once, never AT probes.
 * A matching final PUBACK is always required; no payload retry is performed. */
bool Net_PublishFixed(const NetPublishIo *io,const char *topic,const char *payload,
                      uint16_t id,bool retain,bool *missing_prompt_recovery);
#endif
