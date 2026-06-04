#include "aw21009.h"
#include "rtconfig.h"
#include "ulog.h"
#include <string.h>

static struct rt_i2c_bus_device *i2c_bus = RT_NULL;

// I2C写寄存器函数
static rt_err_t aw21009_write_reg(rt_uint8_t reg, rt_uint8_t value)
{
    rt_size_t ret = 0;
    uint8_t length = sizeof(value);
    uint8_t *buf = rt_malloc(length);
    if (!buf)
    {
        return -RT_ERROR;
    }
    // memcpy(&buf[0], value, length);
    buf[0] = value;
    ret = rt_i2c_mem_write(i2c_bus, AW21009_I2C_ADDR, reg, 1, buf, length);
    rt_free(buf);
    if (ret != length)
    {
        LOG_E("write reg addr and data failed");
        return -RT_ERROR;
    }
    return RT_EOK;
}

static rt_err_t aw21009_read_reg(rt_uint8_t reg, rt_uint8_t *value)
{
    rt_size_t ret = 0;
    uint8_t length = sizeof(value);
    ret = rt_i2c_mem_read(i2c_bus, AW21009_I2C_ADDR, reg, 1, value, length);
    if (ret != length)
    {
        LOG_E("read reg data failed");
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t aw21009_set_led_brightness(rt_uint8_t led, rt_uint16_t brightness)
{
    rt_uint8_t reg_l, reg_h;

    if (led < 1 || led > 6)
    {
        LOG_E("Invalid LED number: %d (must be 1-6)", led);
        return -RT_ERROR;
    }

    // 标准12位模式的正确寄存器映射
    switch (led)
    {
    case 1:
        reg_l = REG_BR00L;
        reg_h = REG_BR00H;
        break;
    case 2:
        reg_l = REG_BR01L;
        reg_h = REG_BR01H;
        break;
    case 3:
        reg_l = REG_BR02L;
        reg_h = REG_BR02H;
        break;
    case 4:
        reg_l = REG_BR03L;
        reg_h = REG_BR03H;
        break;
    case 5:
        reg_l = REG_BR04L;
        reg_h = REG_BR04H;
        break;
    case 6:
        reg_l = REG_BR05L;
        reg_h = REG_BR05H;
        break;
    default:
        return -RT_ERROR;
    }

    // 写入12位亮度值
    aw21009_write_reg(reg_l, brightness & 0xFF);        // 低8位
    aw21009_write_reg(reg_h, (brightness >> 8) & 0x0F); // 高4位

    // 触发更新
    aw21009_write_reg(REG_UPDATE, 0x00);

    // LOG_D("LED%d brightness = %d (0x%03X)", led, brightness, brightness);

    return RT_EOK;
}

// 设置所有LED亮度
rt_err_t aw21009_set_all_brightness(rt_uint16_t brightness)
{
    rt_uint8_t val_l, val_h;

    // 将12位亮度值拆分为高4位和低8位
    val_l = brightness & 0xFF;        // 低8位
    val_h = (brightness >> 8) & 0x0F; // 高4位

    // 设置LED1-LED3的亮度寄存器
    aw21009_write_reg(REG_BR00L, val_l);
    aw21009_write_reg(REG_BR00H, val_h);
    aw21009_write_reg(REG_BR01L, val_l);
    aw21009_write_reg(REG_BR01H, val_h);
    aw21009_write_reg(REG_BR02L, val_l);
    aw21009_write_reg(REG_BR02H, val_h);
    aw21009_write_reg(REG_BR03L, val_l);
    aw21009_write_reg(REG_BR03H, val_h);
    aw21009_write_reg(REG_BR04L, val_l);
    aw21009_write_reg(REG_BR04H, val_h);
    aw21009_write_reg(REG_BR05L, val_l);
    aw21009_write_reg(REG_BR05H, val_h);

    // 触发更新
    aw21009_write_reg(REG_UPDATE, 0x00);

    return RT_EOK;
}

// 初始化AW21009
static rt_err_t aw21009_init(void)
{
    rt_uint8_t val, chip_id;

    rt_kprintf("Initializing AW21009 LED driver...\n");

    // 查找I2C总线设备，根据实际设备名修改
    i2c_bus =
        (struct rt_i2c_bus_device *)rt_device_find(KEY_BOARD_I2C_BUS_NAME);
    if (i2c_bus == RT_NULL)
    {
        rt_kprintf("Failed to find I2C bus device!\n");
        return -RT_ERROR;
    }

    if (rt_device_open((rt_device_t)i2c_bus, RT_DEVICE_OFLAG_RDWR) !=
        RT_EOK)
    {
        LOG_E("open %s device failed", KEY_BOARD_I2C_BUS_NAME);
        return -RT_ERROR;
    }
    else
    {
        LOG_I("open %s device success", KEY_BOARD_I2C_BUS_NAME);
    }

    struct rt_i2c_configuration configuration = {
        .mode = 0,
        .addr = 0,
        .timeout = 500,   // Waiting for timeout period (ms)
        .max_hz = 400000, // I2C rate (hz)
    };
    rt_i2c_configure(i2c_bus, &configuration);

    if (aw21009_read_reg(REG_RESET, &chip_id) == RT_EOK)
    {
        rt_kprintf("AW21009 Chip ID: 0x%02X\n", chip_id);
        if (chip_id != AW21009_CHIP_ID)
        {
            rt_kprintf("Warning: Unexpected Chip ID! Expected 0x%02X\n",
                       AW21009_CHIP_ID);
            return -RT_ERROR;
        }
    }
    else
    {
        rt_kprintf("Failed to read chip ID!\n");
        return -RT_ERROR;
    }

    // 软件复位
    aw21009_write_reg(REG_RESET, 0x00);
    rt_thread_mdelay(5); // 等待复位完成

    // 配置全局控制寄存器2
    // SBMD=1: 单字节模式           bit0
    // RGBMD=0: 禁用RGB模式         bit1
    // UDMD=0: BR和SL更新模式       bit2-3
    // BSDIS=0: i2C从机地址使能     bit4
    // 0000 1000
    aw21009_write_reg(REG_GCR2, 0x08);

    // 配置全局控制寄存器
    // APSE=0: 禁用自动省电
    // CLKFRQ=000: 16MHz
    // PWMRES=10: 12位PWM分辨率
    // CHIPEN=1: 使能芯片
    aw21009_write_reg(REG_GCR, 0x05); // 0000 0101

    // 设置全局电流
    aw21009_write_reg(REG_GCCR, GLOBAL_CURRENT);

    // 设置LED1-LED6的SL寄存器为最大值（255）
    for (rt_uint8_t i = 0; i < 6; i++)
    {
        aw21009_write_reg(REG_SL00 + i, 0xFF);
    }

    // 初始化为全灭
    aw21009_set_all_brightness(LED_BRIGHT_0);

    rt_kprintf("AW21009 initialized successfully!\n");

    // 9. 验证配置
    uint8_t gcr = 0, gccr = 0;
    aw21009_read_reg(REG_GCR, &gcr);
    aw21009_read_reg(REG_GCCR, &gccr);

    rt_kprintf("Verify: GCR=0x%02X (CHIPEN=%d), GCCR=0x%02X\n", gcr, gcr & 1,
               gccr);

    if (!(gcr & 1))
    {
        rt_kprintf("WARNING: CHIPEN=0! Chip not enabled!\n");
    }

    return RT_EOK;
}

// 测试函数 - 呼吸灯效果
static void aw21009_breath_test(void)
{
    rt_uint16_t brightness;
    rt_uint8_t direction = 0; // 0:增加亮度，1:减少亮度

    rt_kprintf("Starting breath test...\n");

    while (1)
    {
        if (direction == 0)
        {
            // 逐渐增加亮度
            for (brightness = 0; brightness < 4095; brightness += 50)
            {
                aw21009_set_all_brightness(brightness);
                rt_thread_mdelay(10);
            }
            direction = 1;
        }
        else
        {
            // 逐渐减少亮度
            for (brightness = 4095; brightness > 0; brightness -= 50)
            {
                aw21009_set_all_brightness(brightness);
                rt_thread_mdelay(10);
            }
            direction = 0;
        }

        // 每完成一次呼吸，切换下一个模式
        static rt_uint8_t pattern = 0;
        pattern = (pattern + 1) % 3;

        // 短暂关闭所有LED
        aw21009_set_all_brightness(LED_BRIGHT_0);
        rt_thread_mdelay(500);
    }
}

// 命令行测试函数
static void aw21009_test(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: aw21009_test <command>\n");
        rt_kprintf("Commands:\n");
        rt_kprintf("  init          Initialize AW21009\n");
        rt_kprintf("  on            Turn all LEDs on\n");
        rt_kprintf("  off           Turn all LEDs off\n");
        rt_kprintf(
            "  set <led> <brightness>  Set specific LED brightness (0-4095)\n");
        rt_kprintf("  breath        Start breath effect\n");
        return;
    }

    if (rt_strcmp(argv[1], "init") == 0)
    {
        aw21009_init();
    }
    else if (rt_strcmp(argv[1], "on") == 0)
    {
        aw21009_set_all_brightness(LED_BRIGHT_100);
        rt_kprintf("All LEDs ON\n");
    }
    else if (rt_strcmp(argv[1], "off") == 0)
    {
        aw21009_set_all_brightness(LED_BRIGHT_0);
        rt_kprintf("All LEDs OFF\n");
    }
    else if (rt_strcmp(argv[1], "set") == 0 && argc == 4)
    {
        rt_uint8_t led = atoi(argv[2]);
        rt_uint16_t brightness = atoi(argv[3]);

        if (led >= 1 && led <= 6)
        {
            aw21009_set_led_brightness(led, brightness);
            rt_kprintf("LED%d brightness set to %d\n", led, brightness);
        }
        else
        {
            rt_kprintf("LED number must be 1-6\n");
        }
    }
    else if (rt_strcmp(argv[1], "breath") == 0)
    {
        // 创建线程运行呼吸效果
        rt_thread_t thread;
        thread =
            rt_thread_create("breath", (void (*)(void *))aw21009_breath_test,
                             RT_NULL, 1024, RT_THREAD_PRIORITY_HIGH, 20);
        if (thread != RT_NULL)
        {
            rt_thread_startup(thread);
        }
    }
    else
    {
        rt_kprintf("Unknown command: %s\n", argv[1]);
    }
}

// 注册到MSH命令
MSH_CMD_EXPORT(aw21009_test, AW21009 LED driver test);

// 应用初始化函数
int aw21009_app_init(void)
{
    // 初始化AW21009
    if (aw21009_init() != RT_EOK)
    {
        rt_kprintf("Failed to initialize AW21009!\n");
        return -RT_ERROR;
    }
    else
    {
        for (rt_uint8_t i = 1; i <= 6; i++)
        {
            aw21009_set_led_brightness(i, 4095);
            rt_thread_mdelay(200);
        }
    }
    return RT_EOK;
}