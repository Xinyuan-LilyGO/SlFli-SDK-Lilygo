#include "LR1121.h"

#include <string.h>

int16_t LR1121_begin(LR1121_t *radio, Module_t *module)
{
    if ((radio == NULL) || (module == NULL)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    memset(radio, 0, sizeof(*radio));
    radio->module = module;
    return RADIOLIB_ERR_UNSUPPORTED;
}
