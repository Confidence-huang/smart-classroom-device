#include "ir_nec_codec.h"
#include <string.h>

/* NEC pulse-distance timings measured falling-edge to falling-edge (microseconds). */
enum {
    NEC_LEADER_MIN_US = 11500U,
    NEC_LEADER_MAX_US = 15500U,
    NEC_REPEAT_MIN_US = 10000U,
    NEC_REPEAT_MAX_US = 11499U,
    NEC_BIT0_MIN_US = 800U,
    NEC_BIT0_MAX_US = 1550U,
    NEC_BIT1_MIN_US = 1800U,
    NEC_BIT1_MAX_US = 2800U,
    NEC_REPEAT_WINDOW_US = 200000U
};

void IrNec_Init(IrNecDecoder *dec)
{
    if (!dec) return;
    memset(dec, 0, sizeof *dec);
}

IrNecStatus IrNec_FeedFallingEdge(IrNecDecoder *dec, uint32_t timestamp_us, IrNecEvent *out)
{
    if (!dec) return IR_NEC_NONE;
    if (!dec->has_edge)
    {
        dec->has_edge = true;
        dec->last_edge_us = timestamp_us;
        return IR_NEC_NONE;
    }

    const uint32_t dt = (uint32_t)(timestamp_us - dec->last_edge_us);
    dec->last_edge_us = timestamp_us;

    if (dt >= NEC_LEADER_MIN_US && dt <= NEC_LEADER_MAX_US)
    {
        dec->receiving = true;
        dec->bit_index = 0U;
        dec->shift_data = 0U;
        return IR_NEC_NONE;
    }

    if (dt >= NEC_REPEAT_MIN_US && dt <= NEC_REPEAT_MAX_US)
    {
        dec->receiving = false;
        if (dec->has_last && (uint32_t)(timestamp_us - dec->last_frame_us) <= NEC_REPEAT_WINDOW_US)
        {
            dec->last_frame_us = timestamp_us;
            if (out)
            {
                *out = dec->last_event;
                out->is_repeat = true;
            }
            return IR_NEC_REPEAT;
        }
        return IR_NEC_NONE;
    }

    if (!dec->receiving) return IR_NEC_NONE;

    uint32_t bit = 0U;
    if (dt >= NEC_BIT0_MIN_US && dt <= NEC_BIT0_MAX_US)
    {
        bit = 0U;
    }
    else if (dt >= NEC_BIT1_MIN_US && dt <= NEC_BIT1_MAX_US)
    {
        bit = 1U;
    }
    else
    {
        dec->receiving = false;
        return IR_NEC_NONE;
    }

    if (bit) dec->shift_data |= (1UL << dec->bit_index);
    ++dec->bit_index;
    if (dec->bit_index < 32U) return IR_NEC_NONE;

    dec->receiving = false;
    const uint32_t raw = dec->shift_data;
    const uint8_t addr = (uint8_t)(raw & 0xFFU);
    const uint8_t cmd = (uint8_t)((raw >> 16) & 0xFFU);
    const uint8_t cmd_inv = (uint8_t)((raw >> 24) & 0xFFU);
    if ((uint8_t)(cmd ^ cmd_inv) != 0xFFU) return IR_NEC_NONE;

    dec->last_event.address = addr;
    dec->last_event.command = cmd;
    dec->last_event.raw = raw;
    dec->last_event.is_repeat = false;
    dec->has_last = true;
    dec->last_frame_us = timestamp_us;
    if (out) *out = dec->last_event;
    return IR_NEC_FRAME;
}
