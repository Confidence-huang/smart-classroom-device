#include "ecovision_algorithm.h"
#include "ecovision_core.h"
#include <string.h>

#define HALF_RANGE 0x80000000u
static const uint32_t stale_ms[EV_CHANNELS]={10000u,10000u,2000u,5000u};
static const int32_t lower[EV_CHANNELS]={-4000,0,0,0};
static const int32_t upper[EV_CHANNELS]={8500,10000,65535,20};
/* Diagnostic thresholds, not physically proven slew limits; never reject a
 * within-range step solely for exceeding them. */
static const int32_t jump[EV_CHANNELS]={200,1000,10000,20};
static int32_t median3(int32_t a,int32_t b,int32_t c) {
    int32_t t;
    if(a>b){t=a;a=b;b=t;} if(b>c){t=b;b=c;c=t;}
    if(a>b){t=a;a=b;b=t;} return b;
}
static void expire(EvChannel *c,uint32_t max_age,uint32_t now) {
    if(!c->has_value){c->age_ms=UINT32_MAX;return;}
    c->age_ms=now-c->at_ms;
    if(c->age_ms>=max_age) {
        c->valid=0; c->stable=0; c->used=0; c->next=0; c->reason|=EV_STALE;
    }
}
/* Returns 1 valid NEW sample, -1 new invalid sample, 0 absent/duplicate/old. */
static int update(EvChannel *c,const EvSample *x,uint32_t now,uint32_t max_age,
                  int32_t min_value,int32_t max_value,int32_t max_jump,int median) {
    uint32_t delta;
    int32_t diff;
    expire(c,max_age,now);
    if(!x->present)return 0;
    c->last_attempt=*x;
    if(now-x->at_ms>=HALF_RANGE) {c->reason|=EV_TIME;return 0;}
    if(c->has_seen) {
        delta=x->at_ms-c->seen_ms;
        if(!delta){c->reason|=EV_DUPLICATE;return 0;}
        if(delta>=HALF_RANGE){c->reason|=EV_TIME;return 0;}
    }
    c->has_seen=1; c->seen_ms=x->at_ms;
    c->reason=0; c->driver_reason=x->driver_reason;
    if(!x->valid)c->reason|=EV_DRIVER;
    if(x->value<min_value||x->value>max_value)c->reason|=EV_RANGE;
    if(now-x->at_ms>=max_age)c->reason|=EV_STALE;
    if(c->reason) {
        c->valid=0;c->stable=0;c->used=0;c->next=0;
        return -1; /* keep last valid raw/value timestamp and provenance */
    }
    if(c->valid) {
        diff=x->value-c->raw;
        if(diff>max_jump||diff< -max_jump)c->reason|=EV_JUMP;
    }
    c->raw=x->value;c->at_ms=x->at_ms;c->age_ms=now-x->at_ms;
    c->source=x->source;c->simulated=x->simulated;
    c->has_value=1;c->valid=1;
    c->history[c->next]=x->value;c->next=(uint8_t)((c->next+1u)%3u);
    if(c->used<3)c->used++;
    c->stable=(uint8_t)(c->used==3);
    c->filtered=(!median||!c->stable)?x->value:
        median3(c->history[0],c->history[1],c->history[2]);
    return 1;
}
void ev_init(EvState *s) {
    unsigned i; memset(s,0,sizeof *s);
    s->comfort.cold_c100=2000;s->comfort.hot_c100=2800;
    s->comfort.dry_rh100=3000;s->comfort.wet_rh100=7000;
    for(i=0;i<EV_CHANNELS;i++) {
        s->channel[i].reason=EV_MISSING;s->channel[i].age_ms=UINT32_MAX;
    }
}
void ev_reset_vision(EvState *s) {
    EvChannel *v=&s->channel[EV_VISION];
    memset(v,0,sizeof *v);v->reason=EV_MISSING;v->age_ms=UINT32_MAX;
    s->occupancy=EV_UNKNOWN;s->positive=0;s->zero_active=0;s->dwell_active=0;
    /* Do not clear fault timer, manual state, other channels or outputs. */
}
void ev_ingest_vision(EvState *s,const EvSample *sample,uint32_t now) {
    EvChannel *v=&s->channel[EV_VISION];
    const int lost=!v->valid||now-v->at_ms>=5000u;
    const int fresh=update(v,sample,now,5000u,0,20,20,0);
    if(lost||!v->valid) {
        s->occupancy=EV_UNKNOWN;s->positive=0;s->zero_active=0;s->dwell_active=0;
    }
    if(fresh>0) {
        if(v->raw>0) {
            s->zero_active=0;
            if(s->positive<3)s->positive++;
            if(s->positive>=3)s->occupancy=EV_OCCUPIED;
        } else {
            s->positive=0;
            if(!s->zero_active){s->zero_active=1;s->zero_since=v->at_ms;}
            if(v->at_ms-s->zero_since>=30000u)s->occupancy=EV_VACANT;
        }
    }
}
void ev_step(EvState *s,const EvInput *in,EvOutput *o) {
    unsigned i; int target=-1;
    EvChannel *v=&s->channel[EV_VISION],*t=&s->channel[EV_TEMP];
    if(!t->valid||in->now_ms-t->at_ms>=10000u)s->dwell_active=0;
    memset(o,0,sizeof *o);
    ev_ingest_vision(s,&in->sample[EV_VISION],in->now_ms);
    for(i=0;i<EV_CHANNELS;i++) {
        if(i!=EV_VISION)
            update(&s->channel[i],&in->sample[i],in->now_ms,
                   stale_ms[i],lower[i],upper[i],jump[i],1);
        if(!s->channel[i].valid || !s->channel[i].stable ||
           s->channel[i].reason) o->degraded_mask|=(uint8_t)(1u<<i);
        if(s->channel[i].has_value&&s->channel[i].simulated)
            o->simulated_mask|=(uint8_t)(1u<<i);
    }
    o->occupancy=s->occupancy;o->vision_valid=v->valid;o->temp_valid=t->valid;
    o->reasons|=EV_REASON_LIGHT_UNCALIBRATED;
    if(s->occupancy==EV_UNKNOWN||!v->valid) {
        o->affected_mask|=3;o->reasons|=EV_REASON_OCCUPANCY_UNKNOWN;
    }
    if(!t->valid){o->affected_mask|=1;o->reasons|=EV_REASON_TEMP_UNKNOWN;}
    if(t->valid) o->thermal=(uint8_t)(t->filtered<s->comfort.cold_c100?EV_COLD:
        t->filtered>=s->comfort.hot_c100?EV_HOT:EV_SUITABLE);
    if(s->channel[EV_RH].valid) {
        int32_t rh=s->channel[EV_RH].filtered;
        o->humidity_need=(uint8_t)(rh<s->comfort.dry_rh100?EV_DRY:
            rh>s->comfort.wet_rh100?EV_WET:EV_RH_SUITABLE);
    }
    if(!v->valid||!t->valid) {
        if(!s->fault_active){s->fault_active=1;s->fault_since=in->now_ms;}
        o->reasons|=EV_REASON_FAULT;
    } else s->fault_active=0;
    o->fan=(uint8_t)(in->fan_actual!=0);o->led=(uint8_t)(in->led_actual!=0);
    if(in->mode!=s->previous_mode)s->dwell_active=0;
    s->previous_mode=in->mode;
    if(in->mode==EV_LOCAL_OFF) {
        o->fan=0;o->led=0;o->apply_mask=3;o->reasons|=EV_REASON_LOCAL;
        s->dwell_active=0;return;
    }
    if(in->mode!=EV_AUTO) {
        o->reasons|=EV_REASON_MANUAL;s->dwell_active=0;return;
    }
    o->apply_mask=3;
    if(s->fault_active&&in->now_ms-s->fault_since>=60000u) {
        o->fan=0;o->led=0;s->dwell_active=0;return;
    }
    if(s->occupancy==EV_VACANT&&v->valid) {
        o->fan=0;o->led=0;o->reasons|=EV_REASON_VACANT;s->dwell_active=0;return;
    }
    if(s->occupancy==EV_OCCUPIED&&v->valid) {
        o->led=1;o->reasons|=EV_REASON_OCCUPIED;
        if(t->valid)target=t->raw>=2800?1:t->raw<=2700?0:-1;
    }
    if(target<0||target==o->fan){s->dwell_active=0;return;}
    if(!s->dwell_active||s->dwell_target!=(uint8_t)target) {
        s->dwell_active=1;s->dwell_target=(uint8_t)target;s->dwell_since=in->now_ms;
    }
    if(in->now_ms-s->dwell_since>=5000u) {
        o->fan=(uint8_t)target;o->reasons|=EV_REASON_TEMP;s->dwell_active=0;
    }
}
int ev_debounce(EvDebounce *s,uint32_t now,uint8_t level,uint32_t delay_ms) {
    level=(uint8_t)(level!=0);
    if(!s->initialized||level!=s->candidate) {
        s->initialized=1;s->candidate=level;s->since=now;
    }
    if(now-s->since>=delay_ms){s->stable=level;s->known=1;}
    return s->known?s->stable:-1;
}

enum { LUX_MAX_AGE_MS = 3000, CO2_MAX_AGE_MS = 10000 };

void ev_core_init(EvCoreState *state) {
    memset(state,0,sizeof *state);
    ev_init(&state->base);
    state->lux.reason=state->co2.reason=EV_MISSING;
    state->lux.age_ms=state->co2.age_ms=UINT32_MAX;
}

/* Linear rise, full-score band, linear fall. All products fit int32_t. */
static uint8_t band_score(int32_t value,int32_t low_zero,int32_t low_full,
                          int32_t high_full,int32_t high_zero) {
    if(value<=low_zero||value>=high_zero)return 0;
    if(value<low_full)return (uint8_t)((value-low_zero)*100/(low_full-low_zero));
    if(value>high_full)return (uint8_t)((high_zero-value)*100/(high_zero-high_full));
    return 100;
}

static void core_score(const EvCoreState *state,EvCoreOutput *out) {
    const EvChannel *temp=&state->base.channel[EV_TEMP];
    const EvChannel *rh=&state->base.channel[EV_RH];
    const EvChannel *vision=&state->base.channel[EV_VISION];
    static const uint8_t weights[]={35,15,30,20};
    unsigned lowest=100,sum=0;
    memset(out->score,EV_SCORE_UNKNOWN,sizeof out->score);
    out->total_score=out->weighted_score=out->measured_score=EV_SCORE_UNKNOWN;
    if(temp->valid)out->score[EV_SCORE_TEMP]=band_score(temp->filtered,1700,2300,2700,3300);
    if(rh->valid)out->score[EV_SCORE_RH]=band_score(rh->filtered,2000,4000,6000,8000);
    if(state->lux.valid)
        out->score[EV_SCORE_LIGHT]=band_score(state->lux.filtered,10000,30000,50000,100000);
    if(state->co2.valid) {
        int32_t ppm=state->co2.filtered;
        out->score[EV_SCORE_AIR]=(uint8_t)(ppm<=800?100:ppm>=1500?0:(1500-ppm)*100/700);
    }
    out->people_valid=vision->valid;
    if(vision->valid)out->people_count=(uint8_t)vision->raw;
    for(unsigned i=0;i<EV_SCORE_COUNT;i++) {
        unsigned score=out->score[i];
        out->impact_x100[i]=UINT16_MAX;
        if(score==EV_SCORE_UNKNOWN)continue;
        out->score_mask|=(uint8_t)(1u<<i);
        out->coverage_pct+=weights[i];
        sum+=score*weights[i];
        if(score<lowest)lowest=score;
        if(out->people_valid)out->impact_x100[i]=(uint16_t)(out->people_count*(100u-score));
    }
    if(out->coverage_pct) {
        unsigned average=sum/out->coverage_pct;
        out->measured_score=(uint8_t)(average<lowest+20?average:lowest+20);
        if(out->coverage_pct==100) {
            out->weighted_score=(uint8_t)average;
            out->total_score=out->measured_score;
        }
    }
}

static int confirmed(uint8_t *pending,uint32_t *since,uint8_t candidate,
                     uint32_t now,uint32_t duration) {
    if(*pending!=candidate){*pending=candidate;*since=now;}
    return candidate&&now-*since>=duration;
}

static int ready(const EvCoreInput *input,unsigned device) {
    return (input->ready_mask&(1u<<device))!=0;
}

static uint8_t daylight_led(EvCoreState *state,uint32_t now,uint8_t occupancy,
                            uint8_t mode,uint8_t lit,uint8_t policy,uint8_t force_off) {
    uint8_t candidate=EV_HOLD;
    if(policy!=state->previous_daylight_switching||lit!=state->previous_led_actual)
        state->daylight_pending=EV_HOLD;
    state->previous_daylight_switching=policy;
    state->previous_led_actual=lit;

    if(policy!=EV_DAYLIGHT_SHARED_SENSOR&&policy!=EV_DAYLIGHT_ISOLATED_SENSOR) {
        state->daylight_pending=EV_HOLD;
        return EV_HOLD;
    }
    if(mode!=EV_AUTO) {
        state->daylight_pending=EV_HOLD;
        return mode==EV_LOCAL_OFF?EV_OFF:EV_HOLD;
    }
    if(occupancy==EV_VACANT||force_off) {
        state->daylight_pending=EV_HOLD;
        return EV_OFF;
    }
    if(occupancy==EV_OCCUPIED&&state->lux.valid&&state->lux.stable) {
        if(!lit&&state->lux.filtered<28000)candidate=EV_ON;
        if(lit&&policy==EV_DAYLIGHT_ISOLATED_SENSOR&&state->lux.filtered>38000)candidate=EV_OFF;
    }
    const uint32_t dwell=candidate==EV_ON?10000u:30000u;
    return confirmed(&state->daylight_pending,&state->daylight_since,candidate,now,dwell)?
           candidate:EV_HOLD;
}

uint8_t ev_daylight_observe(EvCoreState *state,const EvSample *lux,uint32_t now,
                            uint8_t occupancy,uint8_t mode,uint8_t lit,uint8_t policy,
                            uint8_t force_off) {
    EvSample sample=*lux;
    if(sample.simulated)sample.valid=0;
    if(!state->lux.valid||now-state->lux.at_ms>=LUX_MAX_AGE_MS)
        state->daylight_pending=EV_HOLD;
    update(&state->lux,&sample,now,LUX_MAX_AGE_MS,0,10000000,15000,1);
    return daylight_led(state,now,occupancy,mode,lit,policy,force_off);
}

static void core_advice(EvCoreState *state,const EvCoreInput *input,EvCoreOutput *out) {
    const uint32_t now=input->base.now_ms;
    const EvChannel *temp=&state->base.channel[EV_TEMP];
    const int occupied=out->base.occupancy==EV_OCCUPIED;
    const int vacant=out->base.occupancy==EV_VACANT;
    const int window_closed=input->window_closed_confirmed&&!input->window_open_confirmed;
    const int window_open=input->window_open_confirmed&&!input->window_closed_confirmed;
    const int ac_off=input->ac_off_confirmed&&!input->ac_cooling_confirmed;
    const int ac_cooling=input->ac_cooling_confirmed&&!input->ac_off_confirmed;
    uint8_t light_candidate=EV_HOLD;
    if(input->base.mode!=EV_AUTO) {
        state->light_pending=state->ac_pending=state->ac_demand=0;
        return;
    }
    if(ready(input,EV_DEVICE_FAN)&&(out->base.apply_mask&1u))
        out->advice[EV_DEVICE_FAN]=out->base.fan?EV_ON:EV_OFF;

    if(occupied&&state->lux.valid) {
        if(state->lux.filtered<28000)light_candidate=EV_INCREASE;
        if(state->lux.filtered>38000)light_candidate=EV_DECREASE;
    }
    const int change_light=confirmed(&state->light_pending,&state->light_since,
                                     light_candidate,now,light_candidate==EV_INCREASE?10000u:30000u);
    if(vacant&&ready(input,EV_DEVICE_LIGHT))out->advice[EV_DEVICE_LIGHT]=EV_OFF;
    else if(occupied&&input->daylight==EV_SHADE_NEEDED&&ready(input,EV_DEVICE_CURTAIN))
        out->advice[EV_DEVICE_CURTAIN]=EV_SHADE;
    else if(change_light) {
        if(light_candidate==EV_INCREASE&&input->daylight==EV_DAYLIGHT_ALLOWED&&ready(input,EV_DEVICE_CURTAIN))
            out->advice[EV_DEVICE_CURTAIN]=EV_OPEN;
        else if(ready(input,EV_DEVICE_LIGHT))out->advice[EV_DEVICE_LIGHT]=light_candidate;
    }

    if(!occupied||!temp->valid)state->ac_demand=0;
    uint8_t ac_candidate=0;
    if(occupied&&temp->valid) {
        if(!state->ac_demand&&temp->filtered>=2900)ac_candidate=EV_COOL;
        if(state->ac_demand&&temp->filtered<=2700)ac_candidate=EV_STOP_COOLING;
        /* An externally started unit also needs the low-temperature exit rule. */
        if(ac_cooling&&temp->filtered<=2700)ac_candidate=EV_STOP_COOLING;
    }
    if(confirmed(&state->ac_pending,&state->ac_since,ac_candidate,now,300000u))
        state->ac_demand=(uint8_t)(ac_candidate==EV_COOL);

    if(state->vent_demand) {
        out->ventilation_blocked=(uint8_t)(!input->outdoor_open_permitted||
                                         (!window_open&&(!window_closed||!ready(input,EV_DEVICE_WINDOW)||!ac_off)));
        if(!input->outdoor_open_permitted) {
            if(window_open&&ready(input,EV_DEVICE_WINDOW))out->advice[EV_DEVICE_WINDOW]=EV_CLOSE;
            return;
        }
        if(window_closed&&ready(input,EV_DEVICE_WINDOW)) {
            if(ac_off)out->advice[EV_DEVICE_WINDOW]=EV_OPEN;
            else if(ac_cooling&&ready(input,EV_DEVICE_AC))out->advice[EV_DEVICE_AC]=EV_STOP_COOLING;
        }
        return;
    }
    if(window_open&&state->vent_cleared&&ready(input,EV_DEVICE_WINDOW))
        out->advice[EV_DEVICE_WINDOW]=EV_CLOSE;
    if(ready(input,EV_DEVICE_AC)) {
        if(ac_cooling&&(vacant||(ac_candidate==EV_STOP_COOLING&&now-state->ac_since>=300000u)))
            out->advice[EV_DEVICE_AC]=EV_STOP_COOLING;
        else if(ac_off&&occupied&&state->ac_demand&&window_closed)
            out->advice[EV_DEVICE_AC]=EV_COOL;
    }
}

void ev_core_step(EvCoreState *state,const EvCoreInput *input,EvCoreOutput *out) {
    EvInput base=input->base;
    EvSample lux=input->lux_x100,co2=input->co2_ppm;
    uint32_t now=base.now_ms;
    uint8_t air_candidate=0;
    if(state->previous_simulation!=input->simulation)ev_core_init(state);
    state->previous_simulation=input->simulation;
    if(state->previous_mode!=base.mode)state->light_pending=state->ac_pending=state->ac_demand=0;
    state->previous_mode=base.mode;
    if(input->vision_session_changed)ev_reset_vision(&state->base);
    if(!input->simulation) {
        for(unsigned i=0;i<EV_CHANNELS;i++)if(base.sample[i].simulated)base.sample[i].valid=0;
        if(lux.simulated)lux.valid=0;
        if(co2.simulated)co2.valid=0;
    }
    if(!state->co2.valid||now-state->co2.at_ms>=CO2_MAX_AGE_MS) {
        state->air_pending=state->vent_demand=state->vent_cleared=0;
    }
    if(!state->lux.valid||now-state->lux.at_ms>=LUX_MAX_AGE_MS)
        state->light_pending=state->daylight_pending=0;
    if(!state->base.channel[EV_TEMP].valid||now-state->base.channel[EV_TEMP].at_ms>=10000u)
        state->ac_pending=state->ac_demand=0;
    memset(out,0,sizeof *out);
    ev_step(&state->base,&base,&out->base);
    update(&state->lux,&lux,now,LUX_MAX_AGE_MS,0,10000000,15000,1);
    update(&state->co2,&co2,now,CO2_MAX_AGE_MS,0,100000,300,1);
    core_score(state,out);
    out->simulation=input->simulation;
    if(input->simulation)out->base.apply_mask=0;
    if(state->lux.valid)out->light_need=(uint8_t)(state->lux.filtered<30000?EV_DIM:
                                                               state->lux.filtered>50000?EV_BRIGHT:EV_LUX_OK);
    if(state->co2.valid) {
        if(state->co2.filtered>=1000)air_candidate=EV_OPEN;
        if(state->co2.filtered<=850)air_candidate=EV_CLOSE;
        if(air_candidate!=EV_CLOSE)state->vent_cleared=0;
        if(confirmed(&state->air_pending,&state->air_since,air_candidate,now,
                     air_candidate==EV_OPEN?120000u:180000u)) {
            state->vent_demand=(uint8_t)(air_candidate==EV_OPEN);
            state->vent_cleared=(uint8_t)(air_candidate==EV_CLOSE);
        }
        out->air_need=state->vent_demand?EV_VENTILATE:EV_AIR_OK;
    } else state->air_pending=state->vent_demand=state->vent_cleared=0;
    core_advice(state,input,out);
    out->daylight_led=daylight_led(state,now,state->base.occupancy,base.mode,
        (uint8_t)(base.led_actual!=0),input->daylight_switching,
        (uint8_t)(state->base.fault_active&&now-state->base.fault_since>=60000u));
}
