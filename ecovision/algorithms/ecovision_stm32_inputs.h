#ifndef ECOVISION_STM32_INPUTS_H
#define ECOVISION_STM32_INPUTS_H
#include "ecovision_core.h"
#include "app_environment.h"
#include "vision_protocol.h"

typedef struct {
    char link[17], boot[17];
    uint32_t env_at, vision_seq;
    uint8_t have_env, have_vision_seq, vision_active;
} EvStm32Inputs;

/* Initialize adapter to zero. Call with snapshots and HAL_GetTick taken after
 * reading them. This fills only STM32-owned sample fields and the session pulse.
 * A null environment pointer means its first snapshot has not been published. */
void ev_stm32_inputs(EvStm32Inputs *adapter,EvCoreInput *input,
                     const EnvironmentSnapshot *environment,const EvVision *vision);
#endif
