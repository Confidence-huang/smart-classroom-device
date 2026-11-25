#include "ecovision_core.h"
#include "ecovision_stm32_inputs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static EvCoreState state;
static EvCoreInput input;
static EvCoreOutput output;

static EvSample sample(uint32_t at,int32_t value) {
    EvSample result={at,value,1,1,1,0,0};
    return result;
}

static void reset(void) {
    ev_core_init(&state);
    memset(&input,0,sizeof input);
}

static void tick(uint32_t now,int32_t temp,int32_t lux,int32_t co2) {
    input.base.now_ms=now;
    input.base.sample[EV_TEMP]=sample(now,temp);
    input.base.sample[EV_RH]=sample(now,5000);
    input.base.sample[EV_LIGHT]=sample(now,15000);
    input.base.sample[EV_VISION]=sample(now,4);
    input.lux_x100=lux<0?(EvSample){0}:sample(now,lux);
    input.co2_ppm=co2<0?(EvSample){0}:sample(now,co2);
    ev_core_step(&state,&input,&output);
}

static void test_daylight_switching(void) {
    reset();
    input.daylight_switching=EV_DAYLIGHT_SHARED_SENSOR;
    for(uint32_t t=0;t<=40000;t+=1000)tick(t,2500,42000,-1);
    assert(output.daylight_led==EV_HOLD && output.base.led);
    for(uint32_t t=41000;t<=51000;t+=1000)tick(t,2500,20000,-1);
    assert(output.daylight_led==EV_HOLD);
    tick(52000,2500,20000,-1);
    assert(output.daylight_led==EV_ON);
    input.base.led_actual=1;
    for(uint32_t t=53000;t<=95000;t+=1000)tick(t,2500,60000,-1);
    assert(output.daylight_led==EV_HOLD); /* lamp's own light cannot turn it off */

    input.daylight_switching=EV_DAYLIGHT_ISOLATED_SENSOR;
    for(uint32_t t=96000;t<=125000;t+=1000)tick(t,2500,60000,-1);
    assert(output.daylight_led==EV_HOLD);
    tick(126000,2500,60000,-1);
    assert(output.daylight_led==EV_OFF);
    tick(129000,2500,-1,-1);
    assert(output.daylight_led==EV_HOLD); /* stale at 3000 ms */
    tick(130000,2500,60000,-1);
    assert(output.daylight_led==EV_HOLD); /* recovery restarts confirmation */
    input.base.mode=EV_LOCAL_OFF;
    tick(131000,2500,60000,-1);
    assert(output.daylight_led==EV_OFF);
    input.base.mode=EV_REMOTE;
    tick(132000,2500,60000,-1);
    assert(output.daylight_led==EV_HOLD);

    input.base.mode=EV_AUTO;
    for(uint32_t t=133000;t<=163000;t+=1000) {
        input.base.now_ms=t;
        input.base.sample[EV_VISION]=sample(t,0);
        input.base.sample[EV_TEMP]=sample(t,2500);
        ev_core_step(&state,&input,&output);
    }
    assert(output.base.occupancy==EV_VACANT && output.daylight_led==EV_OFF);
    input.daylight_switching=EV_DAYLIGHT_DISABLED;
    tick(164000,2500,42000,-1);
    assert(output.daylight_led==EV_HOLD);
    puts("PASS daylight: occupied bright/dark, self-light, independent sensor, stale, manual and vacancy");
}

int main(void) {
    test_daylight_switching();
    reset();
    tick(0,2500,-1,-1);
    assert(output.coverage_pct==50&&output.total_score==EV_SCORE_UNKNOWN&&output.measured_score==100);
    tick(1000,2500,40000,1500);
    assert(output.total_score==20&&output.weighted_score==70&&output.coverage_pct==100);
    assert(output.impact_x100[EV_SCORE_AIR]==400);
    tick(4000,2500,-1,1500);
    assert(output.coverage_pct==80&&output.score[EV_SCORE_LIGHT]==EV_SCORE_UNKNOWN);

    reset();
    for(uint32_t t=0;t<=120000;t+=1000)tick(t,3000,40000,1500);
    assert(output.air_need==EV_VENTILATE&&output.ventilation_blocked);
    for(unsigned i=0;i<EV_DEVICE_COUNT;i++)assert(output.advice[i]==EV_HOLD);
    input.ready_mask=(1u<<EV_DEVICE_WINDOW)|(1u<<EV_DEVICE_AC);
    input.outdoor_open_permitted=1;
    input.window_closed_confirmed=1;
    input.ac_cooling_confirmed=1;
    tick(121000,3000,40000,1500);
    assert(output.advice[EV_DEVICE_AC]==EV_STOP_COOLING&&output.advice[EV_DEVICE_WINDOW]==EV_HOLD);
    input.ac_cooling_confirmed=0;
    tick(122000,3000,40000,1500);
    assert(output.advice[EV_DEVICE_WINDOW]==EV_HOLD);
    input.ac_off_confirmed=1;
    tick(123000,3000,40000,1500);
    assert(output.advice[EV_DEVICE_WINDOW]==EV_OPEN&&output.advice[EV_DEVICE_AC]==EV_HOLD);
    input.window_closed_confirmed=0;
    input.window_open_confirmed=1;
    for(uint32_t t=124000;t<=304000;t+=1000)tick(t,3000,40000,800);
    assert(output.advice[EV_DEVICE_WINDOW]==EV_HOLD&&output.advice[EV_DEVICE_AC]==EV_HOLD);
    tick(305000,3000,40000,800);
    assert(output.advice[EV_DEVICE_WINDOW]==EV_CLOSE&&output.advice[EV_DEVICE_AC]==EV_HOLD);
    input.window_open_confirmed=0;
    input.window_closed_confirmed=1;
    tick(306000,3000,40000,800);
    assert(output.advice[EV_DEVICE_AC]==EV_COOL);
    input.base.mode=EV_REMOTE;
    tick(307000,3000,40000,800);
    assert(output.advice[EV_DEVICE_AC]==EV_HOLD&&output.base.apply_mask==0);

    reset();
    input.ready_mask=(1u<<EV_DEVICE_LIGHT)|(1u<<EV_DEVICE_CURTAIN);
    input.daylight=EV_DAYLIGHT_ALLOWED;
    for(uint32_t t=0;t<=12000;t+=1000)tick(t,2500,20000,800);
    assert(output.advice[EV_DEVICE_CURTAIN]==EV_OPEN&&output.advice[EV_DEVICE_LIGHT]==EV_HOLD);
    input.ready_mask=1u<<EV_DEVICE_LIGHT;
    tick(13000,2500,20000,800);
    assert(output.advice[EV_DEVICE_LIGHT]==EV_INCREASE);
    input.co2_ppm.simulated=1;
    input.co2_ppm.at_ms=14000;
    input.base.now_ms=14000;
    ev_core_step(&state,&input,&output);
    assert(output.total_score==EV_SCORE_UNKNOWN);
    input.simulation=1;
    ev_core_step(&state,&input,&output);
    assert(output.total_score!=EV_SCORE_UNKNOWN&&output.simulation&&output.base.apply_mask==0);
    input.simulation=0;
    memset(&input.co2_ppm,0,sizeof input.co2_ppm);
    ev_core_step(&state,&input,&output);
    assert(output.total_score==EV_SCORE_UNKNOWN);
    EvStm32Inputs adapter={0};
    EnvironmentSnapshot env={0};
    EvVision vision={0};
    reset();
    env.th_valid=env.lux_valid=true;
    env.temp_x100=2500;env.humi_x100=5000;env.lux_x100=40000;
    env.sampled_at=100;
    vision.active=vision.valid=vision.have_seq=true;
    vision.count=3;vision.fresh_at=90;
    input.base.now_ms=100;
    ev_stm32_inputs(&adapter,&input,&env,&vision);
    assert(input.vision_session_changed&&input.lux_x100.present);
    ev_core_step(&state,&input,&output);
    ev_stm32_inputs(&adapter,&input,&env,&vision);
    assert(!input.vision_session_changed&&!input.lux_x100.present&&!input.base.sample[EV_VISION].present);
    vision.seq++;vision.valid=false;
    input.base.now_ms=110;
    ev_stm32_inputs(&adapter,&input,&env,&vision);
    ev_core_step(&state,&input,&output);
    assert(!output.people_valid&&state.base.channel[EV_VISION].at_ms==90);
    reset();
    for(uint32_t t=10;t<=30;t+=10) {
        const EvSample ordered=sample(t,1);
        ev_ingest_vision(&state.base,&ordered,40);
    }
    input.base.now_ms=40;
    ev_core_step(&state,&input,&output);
    assert(output.base.occupancy==EV_OCCUPIED && output.base.led);
    const EvSample lost={.at_ms=50,.present=1};
    ev_ingest_vision(&state.base,&lost,60);
    const EvSample resumed=sample(60,1);
    ev_ingest_vision(&state.base,&resumed,60);
    input.base.now_ms=60;
    ev_core_step(&state,&input,&output);
    assert(output.base.occupancy==EV_UNKNOWN && state.base.positive==1);
    puts("PASS integrated score, missing data, simulation, lighting and ventilation/AC interlocks");
    return 0;
}
