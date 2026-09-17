#include "Module.h"

#include <board.h>

void RadioLib_boardInitTDisplaySF32(void)
{
#ifdef RADIOLIB_T_DISPLAY_SF32_PINMUX
    HAL_PIN_Set(PAD_PA30, GPIO_A30, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA31, GPIO_A31, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA32, GPIO_A32, PIN_NOPULL, 1);
#endif
}
