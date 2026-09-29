#ifndef RADIOLIB_LR1121_H
#define RADIOLIB_LR1121_H

#include "Module.h"
#include "PhysicalLayer.h"

typedef struct {
    Module_t *module;
    PhysicalLayer_t phy;
    bool initialized;
} LR1121_t;

/* Reserved API. The current implementation returns RADIOLIB_ERR_UNSUPPORTED. */
int16_t LR1121_begin(LR1121_t *radio, Module_t *module);

#endif
