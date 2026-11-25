#include "ir_receiver.h"
#include <string.h>

static void finish(IrReceiver *r) {
    uint8_t cmd, param;
    if (IrPulse_Decode24(&r->frame, &cmd, &param)) {
        IrState *s = &r->state;
        ++s->ok; s->cmd = cmd; s->param = param;
        switch (cmd) {
        case 1: s->on = 1; s->temp = param; break;
        case 2: s->on = 0; break;
        case 3: s->mode = param; break;
        case 4: s->fan = param; break;
        case 5: if (s->temp < 30) ++s->temp; break;
        case 6: if (s->temp > 16) --s->temp; break;
        case 7: s->mode = (uint8_t)(s->mode % 4 + 1); break;
        case 8: s->fan = (uint8_t)(s->fan % 3 + 1); break;
        default: break; /* strict decoder rejects unsupported commands */
        }
    } else ++r->state.bad;
    r->active = 0; r->frame.len = 0;
}
void IrReceiver_Init(IrReceiver *r) {
    memset(r, 0, sizeof *r);
    r->state.temp = 26; r->state.mode = 1; r->state.fan = 1;
}
void IrReceiver_Poll(IrReceiver *r, uint32_t ms) {
    if (r->active && (uint32_t)(ms - r->last_ms) >= 20) finish(r);
}
void IrReceiver_Edge(IrReceiver *r, uint16_t capture, uint32_t ms, unsigned falling) {
    IrReceiver_Poll(r, ms);
    if (!r->active) {
        if (!falling) return; /* first transition must start a low mark */
        r->active = 1; r->frame.len = 0;
    } else {
        uint16_t width = (uint16_t)(capture - r->last_capture);
        if (falling == r->last_falling || width < 100 || width > 15000 ||
            r->frame.len >= IR_PULSE_DEMO_LENGTH) {
            ++r->state.bad; r->active = 0; r->frame.len = 0;
            return;
        }
        r->frame.us[r->frame.len++] = width;
    }
    r->last_ms = ms; r->last_capture = capture; r->last_falling = falling;
}
void IrReceiver_Fault(IrReceiver *r) {
    ++r->state.faults; r->active = 0; r->frame.len = 0;
}
