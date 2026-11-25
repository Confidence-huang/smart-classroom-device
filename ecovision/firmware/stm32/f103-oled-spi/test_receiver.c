#include "ir_receiver.h"
#include <assert.h>
#include <stdio.h>

static uint16_t capture;
static uint32_t ms;
static void frame(IrReceiver *r, const IrPulseFrame *f) {
    unsigned falling = 1;
    IrReceiver_Edge(r, capture, ms, falling);
    for (unsigned i = 0; i < f->len; ++i) {
        capture = (uint16_t)(capture + f->us[i]);
        ms += (f->us[i] + 999u) / 1000u;
        falling ^= 1;
        IrReceiver_Edge(r, capture, ms, falling);
    }
    ms += 21;
    IrReceiver_Poll(r, ms);
    capture = (uint16_t)(capture + 21000u);
}
int main(void) {
    IrReceiver r;
    IrPulseFrame f;
    IrReceiver_Init(&r);
    assert(!r.state.on && r.state.temp == 26 && r.state.ok == 0);
    assert(IrPulse_Encode24(1, 26, &f));
    capture = 63000; ms = 0xfffffff0u; /* timer + millisecond rollover */
    frame(&r, &f);
    assert(r.state.ok == 1 && r.state.on && r.state.temp == 26);
    assert(r.state.cmd == 1 && r.state.param == 26);
    assert(IrPulse_Encode24(5, 0, &f));
    for (unsigned n = 0; n < 8; ++n) frame(&r, &f);
    assert(r.state.temp == 30);
    assert(IrPulse_Encode24(6, 0, &f)); frame(&r, &f);
    assert(r.state.temp == 29);
    assert(IrPulse_Encode24(3, 4, &f)); frame(&r, &f);
    assert(r.state.mode == 4);
    assert(IrPulse_Encode24(7, 0, &f)); frame(&r, &f);
    assert(r.state.mode == 1);
    assert(IrPulse_Encode24(4, 3, &f)); frame(&r, &f);
    assert(IrPulse_Encode24(8, 0, &f)); frame(&r, &f);
    assert(r.state.fan == 1);
    unsigned ok = r.state.ok;
    assert(IrPulse_Encode24(2, 0, &f));
    f.us[49] = f.us[49] == 560 ? 1690 : 560; /* corrupt checksum */
    frame(&r, &f);
    assert(r.state.ok == ok && r.state.on);
    assert(IrPulse_Encode24(2, 0, &f)); f.us[3] = 65535;
    frame(&r, &f);
    assert(r.state.ok == ok && r.state.on);
    assert(IrPulse_Encode24(2, 0, &f)); f.us[50] = 100;
    frame(&r, &f);
    assert(r.state.ok == ok && r.state.on);
    assert(IrPulse_Encode24(2, 0, &f));
    IrReceiver_Edge(&r, capture, ms, 1);
    IrReceiver_Edge(&r, (uint16_t)(capture + 9000), ms + 9, 1); /* wrong edge polarity */
    IrReceiver_Poll(&r, ms + 40); ms += 50;
    assert(r.state.ok == ok);
    frame(&r, &f);
    assert(r.state.ok == ok + 1 && !r.state.on);
    IrReceiver_Edge(&r, capture, ms, 1);
    IrReceiver_Fault(&r);
    assert(r.state.faults == 1 && !r.active);
    assert(IrPulse_Encode24(1, 22, &f)); frame(&r, &f);
    assert(r.state.on && r.state.temp == 22);
    puts("PASS: captured demo, timer/tick wrap, state limits, checksum/timing rejection, polarity/fault recovery");
    return 0;
}
