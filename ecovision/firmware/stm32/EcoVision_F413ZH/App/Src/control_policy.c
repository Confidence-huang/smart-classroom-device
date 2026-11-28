#include "control_policy.h"
#include <limits.h>
#include <string.h>

void Control_Init(ControlPolicy *p) {
    memset(p,0,sizeof *p);
}

void Control_BumpEpoch(ControlPolicy *p) {
    if(p->output.control_epoch==UINT32_MAX) {
        p->output.requires_new_boot=true;
        p->output.mode=CONTROL_LOCAL_OFF;
        p->output.led=false;p->output.fan_level=CONTROL_FAN_OFF;
    } else {
        p->output.control_epoch++;
    }
}

static uint8_t auto_fan_level(const ControlPolicy *p) {
    if(!p->auto_fan && !p->auto_air_fan) return CONTROL_FAN_OFF;
    return p->auto_boost ? CONTROL_FAN_BOOST : CONTROL_FAN_DEFAULT;
}

/* Local takeover cancels the lease even if an old command is retransmitted. */
void Control_LocalOff(ControlPolicy *p) {
    p->output.mode=CONTROL_LOCAL_OFF;
    p->output.led=false;p->output.fan_level=CONTROL_FAN_OFF;
    p->output.hold_remaining_s=0;
    p->auto_led=false;p->auto_fan=false;p->auto_air_fan=false;p->auto_boost=false;
    p->hot_active=false;p->cold_active=false;
    p->air_bad_active=false;p->air_ok_active=false;
    Control_BumpEpoch(p);
}

static void button(ControlPolicy *p,uint32_t now,bool pressed) {
    if(pressed!=p->button_candidate) {
        p->button_candidate=pressed;p->button_changed_at=now;
    }
    if(p->button_candidate!=p->button_stable &&
       (uint32_t)(now-p->button_changed_at)>=30U) {
        p->button_stable=p->button_candidate;
        if(p->button_stable) {
            p->pressed_at=p->button_changed_at;p->long_handled=false;
        } else if(!p->long_handled) {
            /* Classify by the physical release edge, not delayed debounce. */
            if((uint32_t)(p->button_changed_at-p->pressed_at)>=2000U) {
                Control_LocalOff(p);
            } else {
                p->output.mode=CONTROL_AUTO;
                p->output.hold_remaining_s=0;
                p->hot_active=false;p->cold_active=false;
                p->air_bad_active=false;p->air_ok_active=false;
                Control_BumpEpoch(p);
            }
        }
    }
    if(p->button_stable && p->button_candidate && !p->long_handled &&
       (uint32_t)(now-p->pressed_at)>=2000U) {
        Control_LocalOff(p);p->long_handled=true;
    }
}

bool Control_TemperatureFresh(const ControlInput *in) {
    return in->temperature_valid && in->temperature_age_ms<10000U &&
           in->temperature_x100>=-4000 && in->temperature_x100<=8500;
}

void Control_Update(ControlPolicy *p,uint32_t now,const ControlInput *in) {
    button(p,now,in->button_pressed);
    bool vision=in->vision_valid && in->vision_age_ms<5000U && in->count<=20U;
    bool temperature=Control_TemperatureFresh(in);
    if(!vision || in->new_vision_session) {
        p->output.occupancy=CONTROL_UNKNOWN;
        p->positives=0;p->zero_active=false;
    }
    if(vision && in->new_vision_frame) {
        if(in->count>0U) {
            p->zero_active=false;
            if(p->positives<3U)p->positives++;
            if(p->positives>=3U)p->output.occupancy=CONTROL_OCCUPIED;
        } else {
            p->positives=0;
            if(!p->zero_active){p->zero_active=true;p->zero_at=now;}
            if((uint32_t)(now-p->zero_at)>=30000U)p->output.occupancy=CONTROL_VACANT;
        }
    }

    if(!vision || !temperature) {
        if(!p->fault_active){p->fault_active=true;p->fault_at=now;}
    } else p->fault_active=false;

    if(p->output.mode==CONTROL_LOCAL_OFF || p->output.requires_new_boot) {
        p->output.led=false;p->output.fan_level=CONTROL_FAN_OFF;p->output.hold_remaining_s=0;
        p->hot_active=false;p->cold_active=false;
        p->air_bad_active=false;p->air_ok_active=false;
        return;
    }
    /* Maintain the AUTO decision separately so remote settings never become AUTO's remembered outputs. */
    if(p->output.occupancy==CONTROL_VACANT) {
        p->auto_led=false;p->auto_fan=false;p->auto_air_fan=false;p->auto_boost=false;
        p->hot_active=false;p->cold_active=false;
        p->air_bad_active=false;p->air_ok_active=false;
    } else if(p->output.occupancy==CONTROL_OCCUPIED) {
        p->auto_led=true;
        if(!temperature) {p->hot_active=false;p->cold_active=false;}
        else if(in->temperature_x100>=2800) {
            p->cold_active=false;
            if(!p->hot_active){p->hot_active=true;p->hot_at=now;}
            if((uint32_t)(now-p->hot_at)>=5000U)p->auto_fan=true;
        } else if(in->temperature_x100<=2700) {
            p->hot_active=false;
            if(!p->cold_active){p->cold_active=true;p->cold_at=now;}
            if((uint32_t)(now-p->cold_at)>=5000U)p->auto_fan=false;
        } else {p->hot_active=false;p->cold_active=false;}

        /* Optional SGP30 air-quality ventilation assist with 10s hysteresis dwell. */
        if(!in->air_valid) {
            p->air_bad_active=false;p->air_ok_active=false;p->auto_air_fan=false;
        } else if(in->eco2_ppm>=1200U || in->tvoc_ppb>=400U) {
            p->air_ok_active=false;
            if(!p->air_bad_active){p->air_bad_active=true;p->air_bad_at=now;}
            if((uint32_t)(now-p->air_bad_at)>=10000U)p->auto_air_fan=true;
        } else if(in->eco2_ppm<=900U && in->tvoc_ppb<=250U) {
            p->air_bad_active=false;
            if(!p->air_ok_active){p->air_ok_active=true;p->air_ok_at=now;}
            if((uint32_t)(now-p->air_ok_at)>=10000U)p->auto_air_fan=false;
        } else {p->air_bad_active=false;p->air_ok_active=false;}

        /* Boost AUTO fan to level 4 under severe heat (>=30 C) or poor IAQ, with hysteresis. */
        bool high_heat = temperature && in->temperature_x100 >= 3000;
        bool cool_enough = !temperature || in->temperature_x100 <= 2900;
        bool poor_air = in->air_valid && (in->eco2_ppm >= 2000U || in->tvoc_ppb >= 660U);
        bool air_cleared = !in->air_valid || (in->eco2_ppm <= 1600U && in->tvoc_ppb <= 500U);
        if(!p->auto_fan && !p->auto_air_fan) p->auto_boost=false;
        else if(high_heat || poor_air) p->auto_boost=true;
        else if(cool_enough && air_cleared) p->auto_boost=false;
    } else {
        p->hot_active=false;p->cold_active=false;
        p->air_bad_active=false;p->air_ok_active=false;
    }
    if(p->fault_active && (uint32_t)(now-p->fault_at)>=60000U) {
        p->auto_led=false;p->auto_fan=false;p->auto_air_fan=false;p->auto_boost=false;
    }

    if(p->output.mode==CONTROL_REMOTE) {
        /* Unsigned elapsed time remains correct across a tick wrap during a short lease. */
        uint32_t elapsed=now-p->remote_started_at;
        if(elapsed<p->remote_duration_ms) {
            p->output.hold_remaining_s=(uint16_t)((p->remote_duration_ms-elapsed+999U)/1000U);
            return;
        }
        p->output.mode=CONTROL_AUTO;
    }
    p->output.hold_remaining_s=0;
    p->output.fan_level=auto_fan_level(p);
    p->output.led=p->auto_led;
}

/* B1 was evaluated in this poll; neither an old epoch nor a current lock can take the outputs. */
bool Control_Request(ControlPolicy *p,uint32_t now,uint32_t epoch,uint16_t hold_s,uint8_t fan_level,bool led) {
    if(p->output.mode==CONTROL_LOCAL_OFF || p->output.requires_new_boot ||
       p->output.control_epoch!=epoch || fan_level>CONTROL_FAN_MAX || (hold_s!=0U && (hold_s<30U || hold_s>300U)))return false;

    p->output.hold_remaining_s=hold_s;
    if(hold_s==0U) {
        p->output.mode=CONTROL_AUTO;
        p->output.fan_level=auto_fan_level(p);
        p->output.led=p->auto_led;
    } else {
        p->output.mode=CONTROL_REMOTE;
        p->remote_started_at=now;p->remote_duration_ms=(uint32_t)hold_s*1000U;
        /* The manual lease owns the selected level until AUTO or local OFF takes over. */
        p->output.fan_level=fan_level;p->output.led=led;
    }
    return true;
}

const char *Control_ModeName(ControlMode mode) {
    if(mode==CONTROL_LOCAL_OFF)return "LOCAL_OFF";
    if(mode==CONTROL_REMOTE)return "REMOTE";
    return "AUTO";
}
