#include "LR2021.h"

#include <string.h>

int16_t LR2021_begin(LR2021_t *radio, Module_t *module)
{
    if ((radio == NULL) || (module == NULL)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    memset(radio, 0, sizeof(*radio));
    radio->module = module;
    return RADIOLIB_ERR_UNSUPPORTED;
}
