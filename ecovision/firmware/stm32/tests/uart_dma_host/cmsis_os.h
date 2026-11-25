#include <assert.h>
extern unsigned critical_depth;
#define taskENTER_CRITICAL() (++critical_depth)
#define taskEXIT_CRITICAL() do {assert(critical_depth);--critical_depth;}while(0)
