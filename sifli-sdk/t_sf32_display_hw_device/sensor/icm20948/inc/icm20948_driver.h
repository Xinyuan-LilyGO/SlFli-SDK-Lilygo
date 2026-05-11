#ifndef __ICM20948_H__
#define __ICM20948_H__

#ifdef __cplusplus__
extern "C"
{
#endif
#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "icm20948_reg.h"

struct icm20948_t{
    icm20948_dev_ctx_t dev_ctx;  // 设备上下文
    icm20948_accel_fs_sel_t accel_fs;  // 加速度计量程
    icm20948_gyro_fs_sel_t gyro_fs;    // 陀螺仪量程
};

// 初始化配置结构体
typedef struct {
    icm20948_accel_fs_sel_t accel_fs;  // 加速度计量程
    icm20948_gyro_fs_sel_t gyro_fs;    // 陀螺仪量程
    icm20948_clksel_t clk_sel;         // 时钟选择
} icm20948_config_t;


typedef struct {
    int16_t AccX;
    int16_t AccY;
    int16_t AccZ;
} ImuAccel;

typedef struct {
    int16_t GyroX;
    int16_t GyroY;
    int16_t GyroZ;
} ImuGyro;

typedef struct {
    int16_t MagX;
    int16_t MagY;
    int16_t MagZ;
} ImuMag;

typedef struct {
    float x;
    float y;
    float z;
} ImuReal;


static int icm20948_init(void);
rt_err_t rt_icm20948_init(void);
extern int32_t icm20948_real_data(ImuReal *accReal, ImuReal *gyroReal);
int32_t icm20948_get_temp_data(float *temp);
void icm20948_enable_i2c_master(icm20948_dev_ctx_t *ctx);
void IMU_GetYawPitchRoll(float *Angles);

int ak09916_init(void);
void ak09916_reset(icm20948_dev_ctx_t *ctx);
uint16_t ak09916_id_get(icm20948_dev_ctx_t *ctx);
extern void ak09916_real_data(ImuReal *magReal);


#ifdef __cplusplus__
extern "C"
}
#endif

#endif /* __ICM20948_H__ */