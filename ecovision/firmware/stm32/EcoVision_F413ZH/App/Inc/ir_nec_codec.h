#ifndef IR_NEC_CODEC_H
#define IR_NEC_CODEC_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    IR_NEC_NONE = 0,
    IR_NEC_FRAME = 1,
    IR_NEC_REPEAT = 2
} IrNecStatus;

typedef struct {
    uint8_t address;
    uint8_t command;
    uint32_t raw;
    bool is_repeat;
} IrNecEvent;

typedef struct {
    bool has_edge;
    bool receiving;
    bool has_last;
    uint8_t bit_index;
    uint32_t last_edge_us;
    uint32_t last_frame_us;
    uint32_t shift_data;
    IrNecEvent last_event;
} IrNecDecoder;

void IrNec_Init(IrNecDecoder *dec);
IrNecStatus IrNec_FeedFallingEdge(IrNecDecoder *dec, uint32_t timestamp_us, IrNecEvent *out);

#endif
