#include "ir_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    IrParser parser = {0};
    IrEvent event;
    uint8_t tx[IR_TX_SIZE];
    const uint8_t code[] = {0x55, 0x55, 0x01};
    const uint8_t tx_expected[] = {0xFA, 0xF1, 0x55, 0x55, 0x01};

    IrBuildTransmit(tx, code);
    assert(memcmp(tx, tx_expected, sizeof tx) == 0);
    puts("TX FA F1 55 55 01");

    assert(!IrParserPush(&parser, 0xF1, 10, &event));
    assert(!IrParserPoll(&parser, 17, &event));
    assert(IrParserPoll(&parser, 18, &event) && event.kind == IR_ACK);
    puts("ACK F1");

    assert(!IrParserPush(&parser, 0xB0, 20, &event));
    assert(IrParserPush(&parser, 0x55, 40, &event) && event.kind == IR_REJECTED);
    assert(!IrParserPush(&parser, 0x55, 41, &event));
    assert(!IrParserPush(&parser, 0x01, 42, &event));
    assert(IrParserPoll(&parser, 50, &event) && event.kind == IR_DECODED);
    assert(memcmp(event.bytes, code, sizeof code) == 0);
    puts("REJECTED B0; RX 55 55 01");

    assert(!IrParserPush(&parser, 0xF1, UINT32_MAX - 1, &event));
    assert(!IrParserPush(&parser, 0xB0, UINT32_MAX, &event));
    assert(!IrParserPush(&parser, 0x00, 0, &event));
    assert(IrParserPoll(&parser, 8, &event) && event.kind == IR_DECODED);
    assert(event.bytes[0] == 0xF1 && event.bytes[1] == 0xB0 && event.bytes[2] == 0);
    puts("RX F1 B0 00 including timestamp wrap");

    assert(!IrParserPush(&parser, 0x55, 100, &event));
    assert(!IrParserPush(&parser, 0x55, 101, &event));
    assert(IrParserPoll(&parser, 109, &event) && event.kind == IR_MALFORMED);
    assert(event.length == 2);
    for (uint32_t time = 200; time < 220; ++time)
    {
        assert(!IrParserPush(&parser, 0xF1, time, &event));
    }
    assert(IrParserPoll(&parser, 227, &event) && event.kind == IR_MALFORMED);
    assert(event.length == IR_RX_CAPTURE_SIZE && event.truncated);
    assert(!IrParserPush(&parser, 0xF1, 230, &event));
    assert(IrParserPoll(&parser, 238, &event) && event.kind == IR_ACK);
    puts("Malformed capture, overflow rejection, and recovery");

    assert(!IrParserPush(&parser, 0xF1, 250, &event));
    assert(!IrParserPush(&parser, 0x55, 251, &event));
    assert(!IrParserPush(&parser, 0x55, 252, &event));
    assert(!IrParserPush(&parser, 0x01, 253, &event));
    assert(IrParserPoll(&parser, 261, &event) && event.kind == IR_MALFORMED);
    puts("Adjacent ACK and payload discarded as ambiguous");
    return 0;
}
