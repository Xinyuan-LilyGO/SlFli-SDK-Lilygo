#ifndef RADIOLIB_LR2021_H
#define RADIOLIB_LR2021_H

#include "Module.h"
#include "PhysicalLayer.h"

typedef struct {
    Module_t *module;
    PhysicalLayer_t phy;
    bool initialized;
} LR2021_t;

/* Reserved API. The current implementation returns RADIOLIB_ERR_UNSUPPORTED. */
int16_t LR2021_begin(LR2021_t *radio, Module_t *module);

#endif
