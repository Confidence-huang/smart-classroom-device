#ifndef ALERT_POLICY_H
#define ALERT_POLICY_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool initialized, vision_fault, automatic, remote;
    uint8_t fan, led;
    uint32_t auto_at, started_at;
    bool auto_announced;
} AlertPolicy;
/* Caller supplies qualified sensor validity and applied electrical outputs. */
static inline unsigned AlertPolicy_Update(AlertPolicy *s, uint32_t now,
        bool vision, bool automatic, bool remote, uint8_t fan, uint8_t led) {
    unsigned events=0;
    if(!s->initialized)s->started_at=now;
    bool fault=!vision && (uint32_t)(now-s->started_at)>=5000u;
    if(fault && !s->vision_fault)events|=1u;
    if(s->initialized) {
        bool changed=fan!=s->fan || led!=s->led;
        if(remote && (!s->remote || changed))events|=2u;
        if(automatic && s->automatic && changed &&
           (!s->auto_announced || (uint32_t)(now-s->auto_at)>=10000u)) {
            events|=4u;s->auto_at=now;s->auto_announced=true;
        }
    }
    s->initialized=true;s->vision_fault=fault;
    s->automatic=automatic;s->remote=remote;s->fan=fan;s->led=led;
    return events;
}
static inline bool AlertHealth_Stale(uint32_t now, uint32_t last) {
    return (uint32_t)(now-last)>=5000u;
}
#endif
