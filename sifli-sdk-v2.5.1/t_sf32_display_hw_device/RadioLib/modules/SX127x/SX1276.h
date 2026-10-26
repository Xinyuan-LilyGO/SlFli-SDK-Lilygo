#ifndef RADIOLIB_SX1276_H
#define RADIOLIB_SX1276_H

#include "Module.h"
#include "PhysicalLayer.h"

typedef struct {
    Module_t *module;
    PhysicalLayer_t phy;
    bool initialized;
} SX1276_t;

/* Reserved API. The current implementation returns RADIOLIB_ERR_UNSUPPORTED. */
int16_t SX1276_begin(SX1276_t *radio, Module_t *module);

#endif
