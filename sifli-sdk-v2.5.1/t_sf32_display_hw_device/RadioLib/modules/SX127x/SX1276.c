#include "SX1276.h"

#include <string.h>

int16_t SX1276_begin(SX1276_t *radio, Module_t *module)
{
    if ((radio == NULL) || (module == NULL)) {
        return RADIOLIB_ERR_INVALID_ARGUMENT;
    }
    memset(radio, 0, sizeof(*radio));
    radio->module = module;
    return RADIOLIB_ERR_UNSUPPORTED;
}
