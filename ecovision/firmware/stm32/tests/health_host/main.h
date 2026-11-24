#include <stdint.h>
typedef struct {uint32_t KR,PR,RLR,SR;} MockIwdg;
typedef struct {uint32_t CSR;} MockRcc;
extern MockIwdg mock_iwdg;extern MockRcc mock_rcc;
#define IWDG (&mock_iwdg)
#define RCC (&mock_rcc)
#define RCC_CSR_IWDGRSTF (1U<<29)
#define __HAL_RCC_CLEAR_RESET_FLAGS() (mock_rcc.CSR=0)
uint32_t HAL_GetTick(void);
