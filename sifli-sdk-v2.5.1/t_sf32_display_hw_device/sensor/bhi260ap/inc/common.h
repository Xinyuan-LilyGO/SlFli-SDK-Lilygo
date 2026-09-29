#ifndef __BHI260AP_COMMON_H__
#define __BHI260AP_COMMON_H__

#ifdef __cplusplus__
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include "bhy2_defs.h"
#include "bhy2_klio_defs.h"
#include "bhy2_swim_defs.h"
#include "bhy2_head_tracker_defs.h"
#include "bhy2_bsec.h"

char *get_sensor_error_text(uint8_t sensor_error);
const char *get_sensor_name(uint8_t sensor_id);
float get_sensor_default_scaling(uint8_t sensor_id);

#ifdef __cplusplus__
}
#endif

#endif