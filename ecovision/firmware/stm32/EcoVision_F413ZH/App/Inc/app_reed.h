#ifndef APP_REED_H
#define APP_REED_H
#include <stdbool.h>
#include <stdint.h>
/* Debounced electrical level only; window endpoint calibration is separate. */
typedef struct {
    uint32_t candidate_since;
    bool started, candidate_high, stable_high, valid;
} AppReed_Filter;
bool AppReed_Update(AppReed_Filter *filter, bool high, uint32_t now);
void AppReed_Poll(void);
typedef struct {
    bool valid;
    bool do_high;
    uint32_t sampled_at;
} AppReed_Snapshot;
/* Task context only. Invalid until debounced, or after 3 s without polling.
 * Electrical level is not a calibrated window position. */
void AppReed_GetSnapshot(AppReed_Snapshot *out);
#endif
