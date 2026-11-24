/* CSV console adapter. All input fields are integers; -1 valid means no new
 * sample. Validity 0 is a NEW failed sample, never a vacant person count.
 * No networking, files, HAL, clocks or actuator calls in the algorithm core. */
#include "ecovision_algorithm.h"
#include <stdio.h>

int main(void) {
    char line[1024]; EvState s; EvInput in={0}; EvOutput o={0};
    unsigned now,at[4],mode,source,sim,i; int value[4],valid[4],fan,led,n;
    ev_init(&s);
    printf("now_ms,occupancy,thermal,light_need,humidity_need,fan,led,apply_mask,reasons,degraded_mask,simulated_mask,affected_mask");
    for(i=0;i<4;i++)printf(",raw%u,filtered%u,valid%u,age%u,stable%u,reason%u,source%u,simulated%u",i,i,i,i,i,i,i,i);
    puts("");
    while(fgets(line,sizeof line,stdin)) {
        if(line[0]=='#'||line[0]=='n')continue;
        n=sscanf(line,"%u,%u,%d,%d,%u,%d,%d,%u,%d,%d,%u,%d,%d,%u,%d,%d,%u,%u",
            &now,&at[0],&value[0],&valid[0],&at[1],&value[1],&valid[1],
            &at[2],&value[2],&valid[2],&at[3],&value[3],&valid[3],&mode,&fan,&led,&source,&sim);
        if(n!=18||mode>EV_LOCAL_OFF||sim>1||source>255||fan< -1||fan>1||led< -1||led>1) {
            fputs("invalid CSV row\n",stderr);return 2;
        }
        in.now_ms=now;in.mode=(uint8_t)mode;
        in.fan_actual=(uint8_t)(fan<0?o.fan:fan);in.led_actual=(uint8_t)(led<0?o.led:led);
        for(i=0;i<4;i++) {
            if(valid[i]< -1||valid[i]>1){fputs("invalid validity\n",stderr);return 2;}
            in.sample[i].present=(uint8_t)(valid[i]>=0);
            in.sample[i].valid=(uint8_t)(valid[i]==1);
            in.sample[i].at_ms=at[i];in.sample[i].value=value[i];
            in.sample[i].source=(uint8_t)source;in.sample[i].simulated=(uint8_t)sim;
        }
        ev_step(&s,&in,&o);
        printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",now,o.occupancy,o.thermal,
            o.light_need,o.humidity_need,o.fan,o.led,o.apply_mask,o.reasons,o.degraded_mask,o.simulated_mask,o.affected_mask);
        for(i=0;i<4;i++) {
            EvChannel *c=&s.channel[i];
            printf(",%d,%d,%u,%u,%u,%u,%u,%u",c->raw,c->filtered,c->valid,
                c->age_ms,c->stable,c->reason,c->source,c->simulated);
        }
        puts("");
    }
    return ferror(stdin)?2:0;
}
