#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H
#include "ir_pulse_codec.h"
typedef struct {
    uint32_t ok, bad, faults;
    uint8_t on, temp, mode, fan, cmd, param;
} IrState;
typedef struct {
    IrState state;
    IrPulseFrame frame;
    uint32_t last_ms;
    uint16_t last_capture;
    unsigned active, last_falling;
} IrReceiver;
void IrReceiver_Init(IrReceiver *r);
void IrReceiver_Edge(IrReceiver *r, uint16_t capture, uint32_t ms, unsigned falling);
void IrReceiver_Poll(IrReceiver *r, uint32_t ms);
void IrReceiver_Fault(IrReceiver *r);
#endif
