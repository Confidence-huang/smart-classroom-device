#include "ecovision_stm32_inputs.h"
#include <string.h>

enum { SOURCE_AHT20=1, SOURCE_PC5, SOURCE_BH1750, SOURCE_K230 };

static EvSample sample(uint32_t at,int32_t value,uint8_t valid,uint8_t source,int error) {
    EvSample result={at,value,1,valid,source,0,(uint16_t)error};
    return result;
}

void ev_stm32_inputs(EvStm32Inputs *adapter,EvCoreInput *input,
                     const EnvironmentSnapshot *environment,const EvVision *vision) {
    memset(input->base.sample,0,sizeof input->base.sample);
    input->lux_x100.present=0;
    input->vision_session_changed=0;
    if(environment&&(!adapter->have_env||adapter->env_at!=environment->sampled_at)) {
        uint32_t at=environment->sampled_at;
        adapter->have_env=1;
        adapter->env_at=at;
        input->base.sample[EV_TEMP]=sample(at,environment->temp_x100,environment->th_valid,
                                          SOURCE_AHT20,environment->sensor_error);
        input->base.sample[EV_RH]=sample(at,environment->humi_x100,environment->th_valid,
                                        SOURCE_AHT20,environment->sensor_error);
        input->base.sample[EV_LIGHT]=sample(at,environment->light_raw16,environment->light_valid,
                                            SOURCE_PC5,0);
        input->lux_x100=sample(at,(int32_t)environment->lux_x100,environment->lux_valid,
                               SOURCE_BH1750,environment->light_error);
    }
    if(!vision||!vision->active) {
        input->vision_session_changed=adapter->vision_active;
        adapter->vision_active=adapter->have_vision_seq=0;
        return;
    }
    if(!adapter->vision_active||memcmp(adapter->link,vision->link,sizeof adapter->link)||
       memcmp(adapter->boot,vision->boot,sizeof adapter->boot)) {
        memcpy(adapter->link,vision->link,sizeof adapter->link);
        memcpy(adapter->boot,vision->boot,sizeof adapter->boot);
        adapter->vision_active=1;
        adapter->have_vision_seq=0;
        input->vision_session_changed=1;
    }
    if(vision->have_seq&&(!adapter->have_vision_seq||adapter->vision_seq!=vision->seq)) {
        adapter->vision_seq=vision->seq;
        adapter->have_vision_seq=1;
        /* fresh_at tracks the last valid frame. A newly observed invalid frame
         * uses observation time, and never refreshes the valid-frame age. */
        uint32_t at=vision->valid?vision->fresh_at:input->base.now_ms;
        input->base.sample[EV_VISION]=sample(at,vision->count,vision->valid,SOURCE_K230,0);
    }
}
