# F103 六针 SPI OLED 独立诊断

2026-11-24。对应用户照片丝印 `Default:4SPI`、GND/VCC/SCL/SDA/RES/DC。尚未确认控制器型号，分别生成 SSD1306 与 SH1106 128×64 候选；不要把任一候选编译通过视作屏幕型号确认或亮屏验证。

## 接线

VCC=模块支持的3.3V，GND共地；SCL=PB6，SDA=PB7，RES=PB8，DC=PB9。使用软件SPI，PB6/PB7不初始化为I2C。六针模块没有引出CS，内部CS/接口配置以模块电路为准，不臆造外部CS接法。保留SWD PA13/PA14。红外PA0暂不启用，蜂鸣器不驱动。本固件只用于F103，不适用于NUCLEO-F413ZH。

## 行为和构建

上电显示 `SPI OLED TEST`、`STM32F103 READY`、所用控制器候选配置及 `IR NOT ENABLED`；PC13按500ms亮灭节拍输出（实物LED存在、极性及亮度需确认）。使用复位默认内部HSI 8MHz，不依赖外部晶振。

运行 `python build.py`，依赖本机 D:/k51/ARM/ARMCLANG 和用户目录 STM32Cube F1 V1.8.7 CMSIS。字体提取自用户授权的wzy包f103_oled.c，不修改原包。链接范围64KiB Flash、20KiB SRAM；校验HEX记录及数据边界，输出build/images.json。`test.cmd`用MSVC在主机采样SPI上升沿，核对MSB优先、DC和复位；这些测试不证明真实GPIO电气时序或屏幕已亮。

诊断HEX会替换F103当前程序，烧录前须正确识别目标型号/容量并按项目规则保留原程序。不得仅凭ST-LINK序号猜测芯片，不能把此诊断固件写入NUCLEO或擦除其配置区。早期连接失败已恢复；当前通过指定外置ST-LINK读到设备ID0x410、中容量系列、64KiB Flash及Cortex-M3。不要为连接故障关闭读保护或执行整片擦除。

## 验证记录

两套候选均编译链接完成，HEX数据地址为[0x08000000,0x080006F8)。哈希见build/images.json。本目录全新创建，原包和NUCLEO固件未改；工作区无Git，本轮未git。确认屏幕控制器、亮屏和红外功能仍需后续实物测试。

### 2026-11-24 烧录及亮屏

用户明确授权烧录。以外置ST-LINK序号37FF71064E573436B54A1043再次核对目标为0x410/64KiB后，备份F103原Flash至本地私有目录（65536字节），SHA256为8c43173369576d26c5e954c4252955534b97d24016afc99f94e4ac083e5d2251。备份不得上传或放入公开仓库。

烧录build/f103-oled-ssd1306.hex，SHA256为efcd7102d2b80363cd80da4bb54792f319427ec30da4a1f047c3aad4a30691d1。CubeProgrammer仅擦写对应第0、1页，下载及读回校验均成功；未操作NUCLEO、选项字节或整片擦除。随后用户确认BOOT0改回0、上电复位，屏幕显示SPI OLED TEST等文字。该证据确认当前接线和SSD1306候选配置可显示，不能单凭兼容显示断言芯片确切型号。红外仍未启用，收发尚未实测。
