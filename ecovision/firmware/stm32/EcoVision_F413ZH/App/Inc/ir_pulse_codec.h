#ifndef IR_PULSE_CODEC_H
#define IR_PULSE_CODEC_H
#include <stdbool.h>
#include <stdint.h>
enum { IR_PULSE_CAPACITY=600, IR_PULSE_DEMO_LENGTH=51 };
typedef struct { uint16_t us[IR_PULSE_CAPACITY]; uint16_t len; } IrPulseFrame;
bool IrPulse_Decode24(const IrPulseFrame *frame,uint8_t *cmd,uint8_t *param);
bool IrPulse_Encode24(uint8_t cmd,uint8_t param,IrPulseFrame *frame);
bool IrPulse_ValidRaw(const IrPulseFrame *frame);
#endif
