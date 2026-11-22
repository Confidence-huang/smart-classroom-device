#include "ir_protocol.h"

#include <string.h>

void IrBuildTransmit(uint8_t frame[IR_TX_SIZE], const uint8_t code[IR_CODE_SIZE])
{
    frame[0] = 0xFA;
    frame[1] = 0xF1;
    memcpy(&frame[2], code, IR_CODE_SIZE);
}

bool IrParserPoll(IrParser *parser, uint32_t now, IrEvent *event)
{
    if (parser->length == 0U || now - parser->last_byte_ms < IR_GAP_MS)
    {
        return false;
    }

    event->kind = IR_MALFORMED;
    event->length = parser->length;
    event->truncated = parser->truncated;
    event->last_byte_ms = parser->last_byte_ms;
    memcpy(event->bytes, parser->bytes, parser->length);
    if (!event->truncated && event->length == IR_CODE_SIZE)
    {
        event->kind = IR_DECODED;
    }
    else if (!event->truncated && event->length == 1U)
    {
        if (event->bytes[0] == 0xF1)
        {
            event->kind = IR_ACK;
        }
        else if (event->bytes[0] == 0xB0)
        {
            event->kind = IR_REJECTED;
        }
    }
    parser->length = 0U;
    parser->truncated = false;
    return true;
}

bool IrParserPush(IrParser *parser, uint8_t byte, uint32_t now, IrEvent *event)
{
    const bool completed = IrParserPoll(parser, now, event);
    if (parser->length < IR_RX_CAPTURE_SIZE)
    {
        parser->bytes[parser->length++] = byte;
    }
    else
    {
        parser->truncated = true;
    }
    parser->last_byte_ms = now;
    return completed;
}
