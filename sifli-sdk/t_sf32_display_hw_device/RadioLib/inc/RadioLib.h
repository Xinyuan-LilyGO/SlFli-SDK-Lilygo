#ifndef RADIOLIB_H
#define RADIOLIB_H

#include "Module.h"
#include "PhysicalLayer.h"
#include "RadioLibTypes.h"

#ifdef RADIOLIB_USING_SX1262
#include "SX1262.h"
#endif
#ifdef RADIOLIB_USING_LR1121
#include "LR1121.h"
#endif
#ifdef RADIOLIB_USING_LR2021
#include "LR2021.h"
#endif
#ifdef RADIOLIB_USING_SX1276
#include "SX1276.h"
#endif

#define RADIOLIB_VERSION_MAJOR 1
#define RADIOLIB_VERSION_MINOR 0
#define RADIOLIB_VERSION_PATCH 0

#endif
