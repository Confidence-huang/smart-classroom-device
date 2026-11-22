#ifndef IR_PROTOCOL_H
#define IR_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

enum { IR_TX_SIZE = 5, IR_CODE_SIZE = 3, IR_RX_CAPTURE_SIZE = 8, IR_GAP_MS = 8 };

typedef enum {
    IR_ACK,
    IR_REJECTED,
    IR_DECODED,
    IR_MALFORMED
} IrEventKind;

typedef struct {
    uint8_t bytes[IR_RX_CAPTURE_SIZE];
    uint8_t length;
    bool truncated;
    uint32_t last_byte_ms;
} IrParser;

typedef struct {
    IrEventKind kind;
    uint8_t bytes[IR_RX_CAPTURE_SIZE];
    uint8_t length;
    bool truncated;
    uint32_t last_byte_ms;
} IrEvent;

void IrBuildTransmit(uint8_t frame[IR_TX_SIZE], const uint8_t code[IR_CODE_SIZE]);
bool IrParserPush(IrParser *parser, uint8_t byte, uint32_t now, IrEvent *event);
bool IrParserPoll(IrParser *parser, uint32_t now, IrEvent *event);

#endif
