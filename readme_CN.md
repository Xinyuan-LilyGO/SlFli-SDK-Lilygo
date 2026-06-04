# T-Display-SF32 SDK

## 📝 项目概述
本项目是 **T-Display-SF32** 的官方 SDK，基于 `SiFli SDK (2.4.0)` 开发，旨在帮助开发者快速上手并进行 T-Display-SF32 的应用开发。

**核心目录说明：**
- `sifli-sdk/customer/boards`：板级配置文件
- `sifli-sdk/examples`：官方外设驱动示例
- `sifli-sdk/t_sf32_display_hw_device`：T-Display-SF32 专属硬件驱动
- `sifli-sdk/middleware`：官方中间件
- `sifli-sdk/external`：官方第三方库

📚 **配套文档：**
- [SiFli Wiki](https://wiki.sifli.com/)
- [SiFli SDK 官方文档](https://docs.sifli.com/projects/sdk/latest/sf32lb52x/index.html)

---

## ✅ 前置条件
在开始开发之前，请确保您的开发环境满足以下要求：

1. **Python**：版本需为 `3.9 - 3.14`，安装时请务必勾选将 Python 添加到系统环境变量（PATH）。
2. **终端工具**：SiFli-SDK 脚本目前仅支持 PowerShell，推荐安装使用 **PowerShell 7**。

---

## 🛠️ 环境搭建与配置

### 1. 获取 SDK
下载本 SDK 并解压到任意目录，例如：`D:\T-Display-SF32\SDK`。

### 2. 配置 PowerShell 终端环境
1. 打开 Windows Terminal，按下 `Ctrl + ','` 打开设置。
2. 点击“添加新的配置文件”，选择“复制 Windows PowerShell”。
3. 修改新配置文件的设置：
   - **名称**：改为 `SiFli-SDK`
   - **启动目录**：选择“使用父进程目录”
   - **命令行**：修改为以下格式（注意将最后的 `export.ps1` 路径替换为您实际解压的 SDK 路径）：
     ```powershell
     %SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe -ExecutionPolicy Bypass -NoExit -File D:\T-Display-SF32\SDK\sifli-sdk\export.ps1
     ```

---

## 🚀 构建与烧录

请使用上一步配置好的 `SiFli-SDK` 终端执行以下操作：

### 1. 进入工程目录
```powershell
cd sifli-sdk\example\rt_driver\project
```

### 2. 编译工程
使用以下命令进行编译（`-j16` 表示使用16线程加速编译，可根据电脑配置调整）：
```powershell
scons --board=t-display-sf32_hcpu -j16
```

### 3. 烧录固件
编译完成后，执行以下批处理文件进入烧录模式，然后根据提示输入设备的端口号即可完成烧录：
```powershell
build_t-display-sf32_hcpu\uart_download.bat
```

### 4. 菜单配置（可选）
如需修改工程配置，可运行以下命令打开 menuconfig 界面：
```powershell
scons --board=t-display-sf32 --menuconfig
```

---

## 📜 许可证信息
本项目遵循相应的开源许可证，具体信息请查看 SDK 源码及 `sifli-sdk` 目录下的 LICENSE 文件。使用 SiFli SDK 开发需遵循 SiFli 官方的相关许可协议。

---

## 💡 其他信息
- **常见问题**：如遇 Python 相关报错，请优先检查 Python 版本及环境变量是否配置正确；如遇编译报错，请检查终端是否为已加载 `export.ps1` 的专用 PowerShell。
- **技术支持**：更多底层驱动及外设使用方法，请参考 `examples` 文件夹内的官方示例代码及官方文档。