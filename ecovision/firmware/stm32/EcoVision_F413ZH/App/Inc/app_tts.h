#ifndef APP_TTS_H
#define APP_TTS_H

#include "tts_codec.h"
#include <stdbool.h>
#include <stdint.h>

/* Thread-safe voice announcement service. Only networkTask polls and talks to EC200U UART6. */
void AppTts_Init(bool watchdog_reset);
void AppTts_SpeakAction(const TtsActionRequest *req, bool interrupt);
void AppTts_SpeakAuto(TtsAutoType auto_event);
void AppTts_UpdateFaults(uint8_t raw_fault_mask);
bool AppTts_ToggleMute(void);
bool AppTts_PollDispatch(bool modem_playing, TtsDispatch *out);

#endif
