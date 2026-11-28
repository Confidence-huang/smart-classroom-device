#include "ir_nec_codec.h"
#include <assert.h>
#include <stdio.h>

static IrNecStatus feed_frame(IrNecDecoder *dec, uint32_t *t, uint8_t addr, uint8_t addr_inv,
                              uint8_t cmd, uint8_t cmd_inv, IrNecEvent *ev)
{
    uint32_t raw = (uint32_t)addr | ((uint32_t)addr_inv << 8) |
                   ((uint32_t)cmd << 16) | ((uint32_t)cmd_inv << 24);
    assert(IrNec_FeedFallingEdge(dec, *t, ev) == IR_NEC_NONE);
    *t += 13500U; /* 9ms mark + 4.5ms space */
    assert(IrNec_FeedFallingEdge(dec, *t, ev) == IR_NEC_NONE);
    IrNecStatus st = IR_NEC_NONE;
    for (unsigned i = 0U; i < 32U; ++i)
    {
        *t += ((raw >> i) & 1U) ? 2250U : 1125U;
        st = IrNec_FeedFallingEdge(dec, *t, ev);
        if (i < 31U) assert(st == IR_NEC_NONE);
    }
    return st;
}

int main(void)
{
    IrNecDecoder dec;
    IrNec_Init(&dec);
    IrNecEvent ev = {0};

    /* 1. Valid Jiangxie 51 remote frame (addr=0x00, cmd=0x45 POWER) across uint32 wrap. */
    uint32_t t = 0xFFFF8000U;
    assert(feed_frame(&dec, &t, 0x00U, 0xFFU, 0x45U, 0xBAU, &ev) == IR_NEC_FRAME);
    assert(ev.address == 0x00U && ev.command == 0x45U && !ev.is_repeat);

    /* 2. Repeat code 40ms after frame -> IR_NEC_REPEAT with same key. */
    t += 40000U;
    assert(IrNec_FeedFallingEdge(&dec, t, &ev) == IR_NEC_NONE);
    t += 11250U;
    assert(IrNec_FeedFallingEdge(&dec, t, &ev) == IR_NEC_REPEAT);
    assert(ev.address == 0x00U && ev.command == 0x45U && ev.is_repeat);

    /* 3. Stale repeat (>200ms) is ignored. */
    t += 250000U;
    assert(IrNec_FeedFallingEdge(&dec, t, &ev) == IR_NEC_NONE);
    t += 11250U;
    assert(IrNec_FeedFallingEdge(&dec, t, &ev) == IR_NEC_NONE);

    /* 4. Corrupted inverted command byte is rejected. */
    t += 50000U;
    assert(feed_frame(&dec, &t, 0x00U, 0xFFU, 0x0CU, 0x00U, &ev) == IR_NEC_NONE);

    /* 5. Noise glitch followed by valid key '1' (0x0C) resynchronizes cleanly. */
    t += 500U;
    assert(IrNec_FeedFallingEdge(&dec, t, &ev) == IR_NEC_NONE);
    t += 30000U;
    assert(feed_frame(&dec, &t, 0x00U, 0xFFU, 0x0CU, 0xF3U, &ev) == IR_NEC_FRAME);
    assert(ev.address == 0x00U && ev.command == 0x0CU && !ev.is_repeat);

    puts("PASS: NEC IR frame, wraparound, repeat, stale repeat, bad CRC, and noise resync");
    return 0;
}
