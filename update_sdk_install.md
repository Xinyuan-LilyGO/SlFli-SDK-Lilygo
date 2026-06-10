# LILYGO T-SF32 SDK Update and Installation

## 1. Download SDK
Download the latest SDK, extract it to any directory (e.g., `D:\SiFli-SDK`), and set up the SDK build environment according to the official documentation [SDK_Install](https://docs.sifli.com/projects/sdk/latest/en/sf32lb52x/quickstart/install/script/windows.html).

## 2. Windows Terminal Quick Configuration
Refer to the [readme.md](./readme.md#2-configure-the-powershell-terminal-environment) file, find the `Windows Terminal` quick configuration, and configure the command line.

## 3. Porting LILYGO T-SF32 Hardware Driver Libraries
### a. Porting Board Files
Copy the `customer\boards\t-display-sf32` and `customer\boards\t-display-sf32-base` folders from the old SDK to the `customer\boards` directory of the new SDK.

Modify the `customer\boards\Kconfig_lcd` file in the new SDK.
#### (1) Find the `choice` block of `BSP_USING_BUILTIN_LCD` and add the following content inside the if statement:
```
    choice
        prompt "Built-in LCD module driver"
        default LCD_USING_ED_LB55DSI17801
    ......
        config LCD_USING_TFT_CO5300_T_SF32
                 bool "2.16 rect QSPI Single-Screen LCD(LCD_480*480_Lilygo_T_SF32_DevKit_Adapter_V1.0)"
                 select LCD_USING_CO5300
                 select TSC_USING_CST922 if BSP_USING_TOUCHD
                 select BSP_LCDC_USING_QADSPI
                 if LCD_USING_TFT_CO5300_T_SF32
                    config LCD_CO5300_VSYNC_ENABLE
                         bool "Enable LCD VSYNC (TE signal)"
                         def_bool y
                 endif
    endchoice

```

#### (2) Find the `LCD_HOR_RES_MAX`, `LCD_VER_RES_MAX`, and `LCD_DPI` blocks of `BSP_USING_BUILTIN_LCD` and add the following content:
```
config LCD_HOR_RES_MAX
        int
        ......
        default 480 if LCD_USING_TFT_CO5300_T_SF32

config LCD_VER_RES_MAX
        int
        ......
        default 480 if LCD_USING_TFT_CO5300_T_SF32

config LCD_DPI
        int
        ......
        default 480 if LCD_USING_TFT_CO5300_T_SF32

```
#### (3) Verify Successful Porting
Open `Windows Terminal` to enter the SDK terminal, navigate to the `T-Display-SF32\examples\empty_prj\project` folder, and execute the following commands:
```
scons --board=t-display-sf32 --menuconfig  
scons --board=t-display-sf32_hcpu -j8  
```
If the menuconfig interface appears and compilation passes, the porting is successful.
![menuconfig](./image/menuconfig.png)
![build](./image/build.png)

### b. Porting Touchscreen Driver Library
Copy the touch driver `customer\peripherals\cst922` from the old SDK to the `customer\peripherals` directory of the new SDK. The display driver uses `LCD_USING_CO5300`.

Modify the `customer\peripherals\Kconfig` file in the new SDK.
#### (1) Find the `BSP_USING_TOUCHD` block and add the following content:
```
# TP driver of LCD module 
......

config TSC_USING_CST922
    bool
    default n
```

#### (2) Verify Successful Porting
Open `Windows Terminal` to enter the SDK terminal, navigate to the `T-Display-SF32\examples\lcd\project` folder, and execute the following commands:
```
scons --board=t-display-sf32 --menuconfig 
```
Enter the menuconfig interface, select `Config LCD on board -> Enable LCD on the board`, choose
![lcd_chiose](./image/lcd_chiose.png)
![lcd_meunconfig](./image/lcd_meunconfig.png)
Press `D` to save and exit, then execute the following build and flash commands:
```
scons --board=t-display-sf32_hcpu -j8  
build_t-display-sf32_hcpu\uart_download.bat
```
If the screen lights up and touch coordinate information appears in the serial output, the porting is successful.

### c. T-SF32-Display Peripheral Driver Library
Copy the `sifli-sdk\t_sf32_display_hw_device` folder from the old SDK to the `sifli-sdk` directory of the new SDK.
Modify the `sifli-sdk\Kconfig.v2` file in the new SDK.
```
# Add third party packages
#SDK configuration	
source "$SIFLI_SDK/t_sf32_display_hw_device/Kconfig"
```
![add Kconfig](./image/add_kconfig_v2.png)

Open `Windows Terminal` to enter the SDK terminal, navigate to the `T-Display-SF32\examples\empty_prj\project` folder, and execute the following commands:
```
scons --board=t-display-sf32 --menuconfig  
```
Enter the menuconfig interface. If `T SF32 Display hardware packages --->` appears with the newly added hardware driver options, the porting is successful.
![third_menuconfig](./image/t-sf32-hardware-packages.png)
