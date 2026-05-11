/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-05-29     juzhango     the first version
 */
#include "icm20948_reg.h"
int32_t icm20948_read_reg(icm20948_dev_ctx_t *ctx, uint8_t reg, uint8_t *data,
                          uint16_t len)
{
    int32_t ret;
    ret = ctx->read_reg(ctx->handle, reg, data, len);
    return ret;
}

int32_t icm20948_write_reg(icm20948_dev_ctx_t *ctx, uint8_t reg, uint8_t *data,
                           uint16_t len)
{
    int32_t ret;
    ret = ctx->write_reg(ctx->handle, reg, data, len);
    return ret;
}

float_t icm20948_accel_lsb_to_mg(int16_t lsb, icm20948_accel_fs_sel_t fs_sel)
{
    float_t scale;

    switch (fs_sel)
    {
    case ICM20948_ACCEL_FS_SEL_2g:
        scale = 0.061f;
        break;
    case ICM20948_ACCEL_FS_SEL_4g:
        scale = 0.122f;
        break;
    case ICM20948_ACCEL_FS_SEL_8g:
        scale = 0.244f;
        break;
    case ICM20948_ACCEL_FS_SEL_16g:
        scale = 0.488f;
        break;
    default:
        scale = 0.0f;
        break;
    }

    return ((float_t)lsb * scale);
}

float_t icm20948_gyro_lsb_to_mdps(int16_t lsb, icm20948_gyro_fs_sel_t fs_sel)
{
    float_t scale;

    switch (fs_sel)
    {
    case ICM20948_GYRO_FS_SEL_250dps:
        scale = 7.63f;
        break;
    case ICM20948_GYRO_FS_SEL_500dps:
        scale = 15.27f;
        break;
    case ICM20948_GYRO_FS_SEL_1000dps:
        scale = 30.5f;
        break;
    case ICM20948_GYRO_FS_SEL_2000dps:
        scale = 61.0f;
        break;
    default:
        scale = 0.0f;
        break;
    }

    return ((float_t)lsb * scale);
}

int32_t icm20948_device_id_get(icm20948_dev_ctx_t *ctx, uint8_t *buff)
{
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_WHO_AM_I, buff, 1);
    return ret;
}

int32_t icm20948_user_bank_set(icm20948_dev_ctx_t *ctx,
                               icm20948_user_bank_t val)
{
    icm20948_reg_bank_sel_t reg_bank_sel;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_REG_BANK_SEL,
                            (uint8_t *)&reg_bank_sel, 1);
    if (ret == 0)
    {
        reg_bank_sel.user_bank = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_REG_BANK_SEL,
                                 (uint8_t *)&reg_bank_sel, 1);
    }
    return ret;
}

int32_t icm20948_user_bank_get(icm20948_dev_ctx_t *ctx,
                               icm20948_user_bank_t *val)
{
    icm20948_reg_bank_sel_t reg_bank_sel;
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_REG_BANK_SEL,
                            (uint8_t *)&reg_bank_sel, 1);
    switch (reg_bank_sel.user_bank)
    {
    case ICM20948_SEL_USER_BANK_0:
        *val = ICM20948_SEL_USER_BANK_0;
        break;
    case ICM20948_SEL_USER_BANK_1:
        *val = ICM20948_SEL_USER_BANK_1;
        break;
    case ICM20948_SEL_USER_BANK_2:
        *val = ICM20948_SEL_USER_BANK_2;
        break;
    case ICM20948_SEL_USER_BANK_3:
        *val = ICM20948_SEL_USER_BANK_3;
        break;
    default:
        break;
    }
    return ret;
}

int32_t icm20948_reset_set(icm20948_dev_ctx_t *ctx, uint8_t val)
{
    icm20948_pwr_mgmt_1_t pwr_mgmt_1;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B0_PWR_MGMT_1, (uint8_t *)&pwr_mgmt_1,
                            1);
    if (ret == 0)
    {
        pwr_mgmt_1.device_reset = val;
        ret = icm20948_write_reg(ctx, ICM20948_B0_PWR_MGMT_1,
                                 (uint8_t *)&pwr_mgmt_1, 1);
    }
    return ret;
}

int32_t icm20948_reset_get(icm20948_dev_ctx_t *ctx, uint8_t *val)
{
    icm20948_pwr_mgmt_1_t pwr_mgmt_1;
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_B0_PWR_MGMT_1, (uint8_t *)&pwr_mgmt_1,
                            1);
    *val = pwr_mgmt_1.device_reset;
    return ret;
}

int32_t icm20948_sleep_set(icm20948_dev_ctx_t *ctx, icm20948_sleep_t val)
{
    icm20948_pwr_mgmt_1_t pwr_mgmt_1;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B0_PWR_MGMT_1, (uint8_t *)&pwr_mgmt_1,
                            1);
    if (ret == 0)
    {
        pwr_mgmt_1.sleep = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B0_PWR_MGMT_1,
                                 (uint8_t *)&pwr_mgmt_1, 1);
    }
    return ret;
}

int32_t icm20948_clksel_set(icm20948_dev_ctx_t *ctx, icm20948_clksel_t val)
{
    icm20948_pwr_mgmt_1_t pwr_mgmt_1;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B0_PWR_MGMT_1, (uint8_t *)&pwr_mgmt_1,
                            1);
    if (ret == 0)
    {
        pwr_mgmt_1.clksel = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B0_PWR_MGMT_1,
                                 (uint8_t *)&pwr_mgmt_1, 1);
    }
    return ret;
}

int32_t icm20948_gyro_smplrt_div_set(icm20948_dev_ctx_t *ctx, uint8_t val)
{
    int32_t ret;
    ret = icm20948_write_reg(ctx, ICM20948_B2_GYRO_SMPLRT_DIV, &val, 1);
    return ret;
}

int32_t icm20948_gyro_dlpfcfg_set(icm20948_dev_ctx_t *ctx,
                                  icm20948_gyro_dlpfcfg_t val)
{
    icm20948_gyro_config_1_t gyro_config_1;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                            (uint8_t *)&gyro_config_1, 1);
    if (ret == 0)
    {
        gyro_config_1.gyro_dlpfcfg = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                                 (uint8_t *)&gyro_config_1, 1);
    }
    return ret;
}

int32_t icm20948_gyro_full_scale_set(icm20948_dev_ctx_t *ctx,
                                     icm20948_gyro_fs_sel_t val)
{
    icm20948_gyro_config_1_t gyro_config_1;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                            (uint8_t *)&gyro_config_1, 1);
    if (ret == 0)
    {
        gyro_config_1.gyro_fs_sel = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                                 (uint8_t *)&gyro_config_1, 1);
    }
    return ret;
}

int32_t icm20948_gyro_full_scale_get(icm20948_dev_ctx_t *ctx,
                                     icm20948_gyro_fs_sel_t *val)
{
    icm20948_gyro_config_1_t gyro_config_1;
    int32_t ret;
    icm20948_user_bank_t user_bank;

    icm20948_user_bank_get(ctx, &user_bank);

    ret = icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_2);

    ret = icm20948_read_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                            (uint8_t *)&gyro_config_1, 1);
    switch (gyro_config_1.gyro_fs_sel)
    {
    case ICM20948_GYRO_FS_SEL_250dps:
        *val = ICM20948_GYRO_FS_SEL_250dps;
        break;
    case ICM20948_GYRO_FS_SEL_500dps:
        *val = ICM20948_GYRO_FS_SEL_500dps;
        break;
    case ICM20948_GYRO_FS_SEL_1000dps:
        *val = ICM20948_GYRO_FS_SEL_1000dps;
        break;
    case ICM20948_GYRO_FS_SEL_2000dps:
        *val = ICM20948_GYRO_FS_SEL_2000dps;
        break;
    default:
        *val = ICM20948_GYRO_FS_SEL_250dps;
        break;
    }

    icm20948_user_bank_set(ctx, user_bank);
    return ret;
}

int32_t icm20948_gyro_fchoice_set(icm20948_dev_ctx_t *ctx,
                                  icm20948_gyro_fchoice_t val)
{
    icm20948_gyro_config_1_t gyro_config_1;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                            (uint8_t *)&gyro_config_1, 1);
    if (ret == 0)
    {
        gyro_config_1.gyro_fchoice = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B2_GYRO_CONFIG_1,
                                 (uint8_t *)&gyro_config_1, 1);
    }
    return ret;
}

#if 0
/* TODO: debug */
int32_t icm20948_gyro_calibration(icm20948_dev_ctx_t *ctx)
{
    int32_t ret;
    int16_t raw_data[3];
    int32_t gyro_bias[3] = {0};
    uint8_t gyro_offset[6] = {0};

    icm20948_user_bank_t user_bank;

    icm20948_user_bank_get(ctx, &user_bank);

    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_0);
    for (int8_t i = 0; i < 100; i++)
    {
        icm20948_angular_rate_raw_get(ctx, raw_data);
        gyro_bias[0] += raw_data[0];
        gyro_bias[1] += raw_data[1];
        gyro_bias[2] += raw_data[2];
    }

    gyro_bias[0] /= 100;
    gyro_bias[1] /= 100;
    gyro_bias[2] /= 100;

    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_2);

    // Construct the gyro biases for push to the hardware gyro bias registers,
    // which are reset to zero upon device startup.
    // Divide by 4 to get 32.9 LSB per deg/s to conform to expected bias input format.
    // Biases are additive, so change sign on calculated average gyro biases
    gyro_offset[0] = (-gyro_bias[0] / 4 >> 8) & 0xFF;
    gyro_offset[1] = (-gyro_bias[0] / 4) & 0xFF;
    gyro_offset[2] = (-gyro_bias[1] / 4 >> 8) & 0xFF;
    gyro_offset[3] = (-gyro_bias[1] / 4) & 0xFF;
    gyro_offset[4] = (-gyro_bias[2] / 4 >> 8) & 0xFF;
    gyro_offset[5] = (-gyro_bias[2] / 4) & 0xFF;

    ret = icm20948_write_reg(ctx, ICM20948_B2_XG_OFFS_USRH, gyro_offset, 6);

    icm20948_user_bank_set(ctx, user_bank);

    return ret;
}

int32_t icm20948_accel_calibration(icm20948_dev_ctx_t *ctx)
{
    int32_t accel_bias[3] = {0};
    int32_t accel_bias_reg[3] = {0};
    uint8_t accel_offset[6] = {0};

    icm20948_user_bank_t user_bank;

    icm20948_user_bank_get(ctx, &user_bank);

    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_0);
    for (int i = 0; i < 100; i++)
    {
        int16_t raw_data[3];

        icm20948_acceleration_raw_get(ctx, raw_data);

        accel_bias[0] += raw_data[0];
        accel_bias[1] += raw_data[1];
        accel_bias[2] += raw_data[2];
    }

    accel_bias[0] /= 100;
    accel_bias[1] /= 100;
    accel_bias[2] /= 100;

    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_1);

    uint8_t mask_bit[3] = {0, 0, 0};
    uint8_t buff[6];
    icm20948_read_reg(ctx, ICM20948_B1_XA_OFFS_H, buff, 6);
    accel_bias_reg[0] = (int32_t)(buff[0] << 8 | buff[1]);
    mask_bit[0] = buff[1] & 0x01;
    accel_bias_reg[1] = (int32_t)(buff[2] << 8 | buff[3]);
    mask_bit[1] = buff[3] & 0x01;
    accel_bias_reg[2] = (int32_t)(buff[4] << 8 | buff[5]);
    mask_bit[2] = buff[5] & 0x01;

	accel_bias_reg[0] -= (accel_bias[0] / 8);
	accel_bias_reg[1] -= (accel_bias[1] / 8);
	accel_bias_reg[2] -= (accel_bias[2] / 8);


	accel_offset[0] = (accel_bias_reg[0] >> 8) & 0xFF;
  	accel_offset[1] = (accel_bias_reg[0])      & 0xFE;
	accel_offset[1] = accel_offset[1] | mask_bit[0];

	accel_offset[2] = (accel_bias_reg[1] >> 8) & 0xFF;
  	accel_offset[3] = (accel_bias_reg[1])      & 0xFE;
	accel_offset[3] = accel_offset[3] | mask_bit[1];

	accel_offset[4] = (accel_bias_reg[2] >> 8) & 0xFF;
	accel_offset[5] = (accel_bias_reg[2])      & 0xFE;
	accel_offset[5] = accel_offset[5] | mask_bit[2];

    icm20948_write_reg(ctx, ICM20948_B1_XA_OFFS_H, &accel_offset[0], 2);
    icm20948_write_reg(ctx, ICM20948_B1_YA_OFFS_H, &accel_offset[2], 2);
    icm20948_write_reg(ctx, ICM20948_B1_ZA_OFFS_H, &accel_offset[4], 2);

    icm20948_user_bank_set(ctx, user_bank);

    return 0;
}
#endif

int32_t icm20948_odr_align_set(icm20948_dev_ctx_t *ctx, uint8_t val)
{
    int32_t ret;
    if (val != 0)
        val = 1;
    ret = icm20948_write_reg(ctx, ICM20948_B2_ODR_ALIGN_EN, (uint8_t *)&val, 1);
    return ret;
}

int32_t icm20948_accel_smplrt_div_2_set(icm20948_dev_ctx_t *ctx, uint8_t val)
{
    int32_t ret;
    ret = icm20948_write_reg(ctx, ICM20948_B2_ACCEL_SMPLRT_DIV_2, &val, 1);
    return ret;
}

int32_t icm20948_accel_dlpfcfg_set(icm20948_dev_ctx_t *ctx,
                                   icm20948_accel_dlpfcfg_t val)
{
    icm20948_accel_config_t accel_config;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                            (uint8_t *)&accel_config, 1);
    if (ret == 0)
    {
        accel_config.accel_dlpfcfg = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                                 (uint8_t *)&accel_config, 1);
    }
    return ret;
}

int32_t icm20948_accel_full_scale_set(icm20948_dev_ctx_t *ctx,
                                      icm20948_accel_fs_sel_t val)
{
    icm20948_accel_config_t accel_config;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                            (uint8_t *)&accel_config, 1);
    if (ret == 0)
    {
        accel_config.accel_fs_sel = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                                 (uint8_t *)&accel_config, 1);
    }
    return ret;
}

int32_t icm20948_accel_full_scale_get(icm20948_dev_ctx_t *ctx,
                                      icm20948_accel_fs_sel_t *val)
{
    icm20948_accel_config_t accel_config;
    int32_t ret;
    icm20948_user_bank_t user_bank;

    icm20948_user_bank_get(ctx, &user_bank);

    ret = icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_2);
    ret = icm20948_read_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                            (uint8_t *)&accel_config, 1);
    switch (accel_config.accel_fs_sel)
    {
    case ICM20948_ACCEL_FS_SEL_2g:
        *val = ICM20948_ACCEL_FS_SEL_2g;
        break;
    case ICM20948_ACCEL_FS_SEL_4g:
        *val = ICM20948_ACCEL_FS_SEL_4g;
        break;
    case ICM20948_ACCEL_FS_SEL_8g:
        *val = ICM20948_ACCEL_FS_SEL_8g;
        break;
    case ICM20948_ACCEL_FS_SEL_16g:
        *val = ICM20948_ACCEL_FS_SEL_16g;
        break;
    default:
        *val = ICM20948_ACCEL_FS_SEL_2g;
        break;
    }

    icm20948_user_bank_set(ctx, user_bank);
    return ret;
}

int32_t icm20948_accel_fchoice_set(icm20948_dev_ctx_t *ctx,
                                   icm20948_accel_fchoice_t val)
{
    icm20948_accel_config_t accel_config;
    int32_t ret;

    ret = icm20948_read_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                            (uint8_t *)&accel_config, 1);
    if (ret == 0)
    {
        accel_config.accel_fchoice = (uint8_t)val;
        ret = icm20948_write_reg(ctx, ICM20948_B2_ACCEL_CONFIG,
                                 (uint8_t *)&accel_config, 1);
    }
    return ret;
}

int32_t icm20948_acceleration_raw_get(icm20948_dev_ctx_t *ctx, int16_t *val)
{
    uint8_t buff[6];
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_B0_ACCEL_XOUT_H, buff, 6);
    val[0] = (int16_t)buff[0];
    val[0] = (val[0] * 256) + (int16_t)buff[1];
    val[1] = (int16_t)buff[2];
    val[1] = (val[1] * 256) + (int16_t)buff[3];
    val[2] = (int16_t)buff[4];
    val[2] = (val[2] * 256) + (int16_t)buff[5];
    return ret;
}

int32_t icm20948_angular_rate_raw_get(icm20948_dev_ctx_t *ctx, int16_t *val)
{
    uint8_t buff[6];
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_B0_GYRO_XOUT_H, buff, 6);
    val[0] = (int16_t)buff[0];
    val[0] = (val[0] * 256) + (int16_t)buff[1];
    val[1] = (int16_t)buff[2];
    val[1] = (val[1] * 256) + (int16_t)buff[3];
    val[2] = (int16_t)buff[4];
    val[2] = (val[2] * 256) + (int16_t)buff[5];
    return ret;
}

int32_t icm20948_temp_dlpfcfg_set(icm20948_dev_ctx_t *ctx,icm20948_temp_dlpfcfg_t val)
{
    int32_t ret;
    uint8_t data = (uint8_t)val;
    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_2);

    ret = icm20948_write_reg(ctx, ICM20948_B2_TEMP_CONFIG,
                                 (uint8_t *)&data, 1);
    
    return ret;
}

int16_t icm20948_temperature_rate_raw_get(icm20948_dev_ctx_t *ctx, int16_t *val)
{
    uint8_t buff[2];
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_B0_TEMP_OUT_H, buff, 2);
    *val = ((int16_t)buff[0] << 8) | buff[1];
    return ret;
}

/*********AK09916**************/
static void ak09916_write_reg(icm20948_dev_ctx_t *ctx, uint8_t RegisterAddress,
                              uint8_t RegisterData)
{
    uint8_t data[1];
    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_3);
    data[0] = AK09916_ADDRESS;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_ADDR, data, 1);
    data[0] = RegisterData;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_DO, data, 1);
    data[0] = RegisterAddress;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_REG, data, 1);
    data[0] = ICM20948_I2C_SLVX_EN;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_CTRL, data, 1);

    data[0] = 0;
    while (!(data[0] & ICM20948_I2C_SLVX_EN))
    {
        icm20948_read_reg(ctx, ICM20948_I2C_SLV4_CTRL, data, 1);
    }
}

static uint8_t ak09916_read_reg(icm20948_dev_ctx_t *ctx,
                                uint8_t RegisterAddress)
{
    uint8_t data[1];
    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_3);

    data[0] = AK09916_ADDRESS | READ;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_ADDR, data, 1);

    data[0] = RegisterAddress;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_REG, data, 1);

    data[0] = ICM20948_I2C_SLVX_EN;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV4_CTRL, data, 1);

    data[0] = 0;
    while (!(data[0] & ICM20948_I2C_SLVX_EN))
    {
        icm20948_read_reg(ctx, ICM20948_I2C_SLV4_CTRL, data, 1);
    }

    data[0] = 0;
    icm20948_read_reg(ctx, ICM20948_I2C_SLV4_DI, data, 1);

    return data[0];
}

static void ak09916_enabel_mag_data_read(icm20948_dev_ctx_t *ctx, uint8_t reg,
                                         uint8_t bytes)
{
    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_3);

    uint8_t data[1];
    data[0] = AK09916_ADDRESS | READ;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV0_ADDR, data, 1);

    data[0] = reg;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV0_REG, data, 1);

    data[0] = bytes | ICM20948_I2C_SLVX_EN;
    icm20948_write_reg(ctx, ICM20948_I2C_SLV0_CTRL, data, 1);
}

void icm20948_enable_i2c_master(icm20948_dev_ctx_t *ctx)
{
    // Enable I2C master
    uint8_t buf[1];

    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_0);
    buf[0] = ICM20948_I2C_MST_EN;
    icm20948_write_reg(ctx, ICM20948_USER_CTRL, buf, 1);

    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_3);
    buf[0] = 0x07;
    icm20948_write_reg(ctx, ICM20948_I2C_MST_CTRL, buf, 1);
}

void ak09916_reset(icm20948_dev_ctx_t *ctx)
{
    ak09916_write_reg(ctx, AK09916_CNTL_3, 0x01);
}

uint16_t ak09916_id_get(icm20948_dev_ctx_t *ctx)
{
    uint8_t data[2] = {0, 0};
    uint16_t id;
    icm20948_user_bank_set(ctx, ICM20948_SEL_USER_BANK_3);
    data[1] = ak09916_read_reg(ctx, AK09916_WIA_1);
    rt_thread_delay(1);
    data[2] = ak09916_read_reg(ctx, AK09916_WIA_2);
    id = (data[1] << 8) | data[2];
    return id;
}

void ak09916_set_mode(icm20948_dev_ctx_t *ctx, AK09916_opMode mode)
{
    ak09916_write_reg(ctx, AK09916_CNTL_2, mode);
    rt_thread_delay(10);
    if (mode != AK09916_PWR_DOWN)
    {
        ak09916_enabel_mag_data_read(ctx, AK09916_HXL, 0x08);
    }
}

int32_t ak09916_magnetometer_raw_get(icm20948_dev_ctx_t *ctx, int16_t *val)
{
    uint8_t buff[6];
    int32_t ret;
    ret = icm20948_read_reg(ctx, ICM20948_EXT_SLV_SENS_DATA_00, buff, 6);
    val[0] = (int16_t)buff[0];
    val[0] = (val[0] * 256) + (int16_t)buff[1];
    val[1] = (int16_t)buff[2];
    val[1] = (val[1] * 256) + (int16_t)buff[3];
    val[2] = (int16_t)buff[4];
    val[2] = (val[2] * 256) + (int16_t)buff[5];
    return ret;
}
