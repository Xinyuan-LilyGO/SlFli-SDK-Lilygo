#ifndef __BHI260AP_H__
#define __BHI260AP_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <rtthread.h>
#include <rtdevice.h>
#include "bhy2.h"
#include "bhy2_defs.h"
#include "bhy2_hif.h"
#include "bhy2_bsec.h"
#include "bhy2_parse.h"
#include <stdbool.h>

typedef enum SensorRemap {
    // Chip top view, upper left corner
    //  -------------
    // | *             |
    // |               |
    // |               |
    // |               |
    // |               |
    //  -------------
    TOP_LAYER_LEFT_CORNER,
    // Chip top view, upper right corner
    //  -------------
    // |             * |
    // |               |
    // |               |
    // |               |
    // |               |
    //  -------------
    TOP_LAYER_RIGHT_CORNER,
    // Chip top view, bottom right corner of the top
    //  -------------
    // |               |
    // |               |
    // |               |
    // |               |
    // |             * |
    //  -------------
    TOP_LAYER_BOTTOM_RIGHT_CORNER,
    // The top view of the chip, the lower left corner of the front bottom
    //  -------------
    // |               |
    // |               |
    // |               |
    // |               |
    // | *             |
    //  -------------
    TOP_LAYER_BOTTOM_LEFT_CORNER,
    // The bottom view of the chip, the upper left corner of the top
    //  -------------
    // | *             |
    // |               |
    // |               |
    // |               |
    // |               |
    //  -------------
    BOTTOM_LAYER_TOP_LEFT_CORNER,
    // The bottom view of the chip, the upper right corner of the top
    //  -------------
    // |             * |
    // |               |
    // |               |
    // |               |
    // |               |
    //  -------------
    BOTTOM_LAYER_TOP_RIGHT_CORNER,
    // The bottom view of the chip, the lower right corner of the bottom
    //  -------------
    // |               |
    // |               |
    // |               |
    // |               |
    // |             * |
    //  -------------
    BOTTOM_LAYER_BOTTOM_RIGHT_CORNER,
    // Chip bottom view, bottom left corner
    //  -------------
    // |               |
    // |               |
    // |               |
    // |               |
    // | *             |
    //  -------------
    BOTTOM_LAYER_BOTTOM_LEFT_CORNER,
}SensorRemap;

struct sensor_data_t {
    uint8_t sensor_id;
    union {
        struct bhy2_data_xyz xyz;      // XYZ data
        struct bhy2_data_quaternion quat; // 四元数数据        
        struct bhy2_data_orientation orient; // 欧拉角数据
        struct bhy2_bsec_air_quality air_quality; // 空气质量数据
        uint8_t step_counter;
    } data;
    uint64_t *timestamp;
    uint8_t status;
};

struct bhy2_data_xyz_float {
    float x;
    float y;
    float z;
};

struct bhy2_data_orientation_float {
    float heading;
    float pitch;
    float roll;
};

struct bhy2_data_quaternion_float {
    float x;
    float y;
    float z;
    float w;
    float accuracy;
};

static int bhi260ap_init(void);
static void bhi260ap_thread_entry(void *parameter);
rt_err_t rt_bhi260ap_init(void);

bool bhi260ap_configure(uint8_t sensor_id, uint8_t sample_rate, uint32_t report_latency_ms);
bool bhi260ap_configure_range(uint8_t sensor_id, uint8_t range);
struct bhy2_virt_sensor_conf bhi260ap_get_configure(uint8_t sensor_id);
float bhi260ap_get_scaling(uint8_t sensor_id);
const char *bhi260ap_get_sensor_name(uint8_t sensor_id);
bool bhi260ap_set_remap_axes(enum SensorRemap remap);
bool bhi260ap_set_interrupt_ctrl(uint8_t data);
int8_t bhi260ap_get_interrupt_ctrl(void);
bool bhi260ap_is_sensor_available(uint8_t sensor_id);
struct bhy2_data_xyz_float bhi260ap_get_acc_sensor_data();
struct bhy2_data_xyz_float bhi260ap_get_gyro_sensor_data();
struct bhy2_data_orientation_float bhi260ap_get_orient_sensor_data();
struct bhy2_data_quaternion_float bhi260ap_get_quaternion_sensor_data();
uint32_t bhi260ap_get_step_counter_sensor_data();
void bhi260ap_thread_pause(void);
void bhi260ap_thread_resume(void);
#ifdef __cplusplus
}
#endif

#endif /* __BHI260AP_H__ */