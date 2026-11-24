#ifndef NET_COMMAND_H
#define NET_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { NET_COMMAND_LIMIT=2048, NET_COMMAND_ID_SIZE=65 };

typedef struct {
    char device[33], boot[33], id[NET_COMMAND_ID_SIZE];
    uint64_t issued_at;
    uint32_t epoch;
    uint16_t hold_s;
    uint8_t ttl_s;
    int8_t fan, led; /* -1 for AUTO, binary outputs otherwise. */
    int8_t fan_level; /* Absolute PWM level, or -1 for AUTO. */
} NetCommand;

typedef enum {
    NET_COMMAND_OK, NET_COMMAND_INVALID, NET_COMMAND_UNSUPPORTED
} NetCommandParse;

/* The modem provides exact length and topic, followed by the JSON wire bytes. */
char *Net_CommandUrc(char *line, const char *topic);
NetCommandParse Net_ParseCommand(const char *json, NetCommand *command);
bool Net_SameCommand(const NetCommand *a, const NetCommand *b);
bool Net_CommandId(const char *value);

#endif
