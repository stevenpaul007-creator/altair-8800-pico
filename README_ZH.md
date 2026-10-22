# Altair2040 —— 在树莓派 Pico (RP2040) 上运行的 Altair 8800 模拟器

这是 Arduino 项目 Altair101a（Intel 8080 CPU 模拟器 + 虚拟前面板，原作者 tigerfarm，
https://github.com/tigerfarm/arduino/tree/master/instructables/Altair101a）的
RP2040（树莓派 Pico）移植版。

8080 模拟器内核基本未作改动。原先在 Mega/Due 上用 74HC595 移位寄存器驱动的前面板
（LED 与开关），在 Pico 上改为由 **四片 PCA9555 I2C 端口扩展芯片** 驱动。控制台与
8080 串行 I/O 走 **USB CDC**；另提供一个可选的辅助 UART（Serial1）。

注意：本工程**没有实装 TF/SD 卡、MP3 播放器、实时时钟**。这些模块只是空桩
（菜单命令存在但不做任何事）。详见下方“未实装项”。

## 1. 硬件

- 树莓派 Pico（RP2040）。
- 4 片 PCA9555（或兼容芯片；PCF8575 不能直接替代）。地址为 0x20、0x21、0x22、0x23
  （通过 A0/A1/A2 引脚设置）。
- 前面板的 LED 与开关接到 PCA9555 的各端口（见第 3 节）。
- USB 线，用于供电、UF2 烧录以及 CDC 控制台。

### 1.1 接线 / 上拉注意事项

- I2C 总线（SDA/GP6、SCL/GP7）：两条线都要接 4.7 kΩ 上拉到 3V3。RP2040 内部上拉
  太弱，I2C 不可靠。
- PCA9555 上的按钮 / 开关输入：PCA9555 **没有内部上拉**。每个按钮引脚都需要外接
  10 kΩ 上拉到 3V3；开关闭合时把引脚短接到 GND（低电平有效）。
- LED 输出：低电平点亮（active LOW）。建议引脚经限流电阻（如 330Ω）接 LED 阴极，
  阳极接 3V3。

## 2. GPIO 分配（RP2040 Pico）

  信号                           GPIO              说明
  USB CDC 控制台 + 8080 串行     USB（不占 GPIO）   程序中为 Serial。
  辅助 UART（Serial1 / UART0）    GP0=TX, GP1=RX     可选外接终端；默认 115200 波特率。
  面板 I2C（Wire1）              GP6=SDA, GP7=SCL   PCA9555 @ 0x20-0x23，400 kHz。引脚可在 Panel9555.h 中改（PANEL_SDA_PIN / PANEL_SCL_PIN）。
  板载 LED                      GP25               程序未使用。
  GP5                           空闲               原为 74HC595 数据引脚；该驱动在 RP2040 上已禁用。

所有前面板 LED 与开关都在 PCA9555 后面，**不额外占用 RP2040 的 GPIO**。其余 GPIO
均可自由使用。

旧的 74HC595 移位寄存器 LED 驱动（lightsStatusAddressData）在 RP2040 上被刻意置空：
如果启用，它会驱动 GP6/GP7，与 I2C 面板冲突。请勿重新启用。

## 3. PCA9555 前面板映射

  芯片   地址   io0（输出，LED，低电平点亮）                io1（输入，低电平有效）
  0x20   0      D0-D7 数据 LED                            数据/地址开关 A0-A7（反向：io1_k -> 第 7-k 位）
  0x21   1      A0-A7 地址 LED                            命令键：EXAMINE, EXAMINE NEXT, DEPOSIT, DEPOSIT NEXT, RESET, CLR, PROTECT, UNPROTECT
  0x22   2      状态 LED：INT,WO,STACK,HLTA,OUT,M1,INP,MEMR   地址开关 A8-A15（反向：io1_k -> 第 15-k 位）
  0x23   3      A8-A15 地址 LED                           io1_0-3：INTE,PROT,WAIT,HLDA 状态 LED；io1_4-7：STOP,RUN,SINGLE STEP,AUX 按钮

- 数据/地址开关（0x20 的 io1、0x22 的 io1）写入 fpAddressToggleWord，位序按上述反向。
  这个 16 位字即 EXAMINE 使用的地址；其低 8 位即 DEPOSIT 写入的数据字节（经典
  Altair 布局）。开关**直接反映其物理位置**（闭合=置位）。
- 命令按钮发送等价的控制台按键：STOP/SINGLE STEP -> 's'，RUN -> 'r'，
  EXAMINE -> 'x'，EXAMINE NEXT -> 'X'，DEPOSIT -> 'p'，DEPOSIT NEXT -> 'P'，
  RESET -> 'R'，CLR -> 'C'。PROTECT/UNPROTECT 切换 status_prot 标志（在 PROT LED
  上显示）。AUX 暂保留（暂无功能）。
- 以上全部定义在 panel_config.h —— 你改面板接线时**只需编辑这一个文件**。

## 4. 编译与烧录

### 4.1 准备

- 安装 arduino-cli，并装入 earlephilhower 的 RP2040 核心：
    arduino-cli core install rp2040:rp2040
- 若 GitHub 下载卡住，请设置 HTTPS_PROXY / HTTP_PROXY。

### 4.2 编译

使用自带的脚本（生成的 UF2 位于 build/）：
    cd Altair2040
    ./build.sh

或直接：
    arduino-cli compile --fqbn rp2040:rp2040:rpipico       --build-property build.extra_flags='-Wno-error=deprecated-declarations -Wno-deprecated-declarations'       Altair2040.ino

extra_flags 用于屏蔽 RP2040 核心对原代码中 Bxxxx 二进制字面量报 -Werror 的问题。

### 4.3 烧录

按住 Pico 上的 BOOTSEL，接上 USB，把 build/Altair2040.ino.uf2 复制到出现的
RPI-RP2 盘。重启后，在 USB CDC 口打开串口终端（115200, 8N1）。

## 5. 串口控制台命令

在 USB CDC 控制台输入（前面板按钮也会产生等价按键）：

    s   停机（RUN 态）/ 单步（WAIT 态）
    r   运行
    x   EXAMINE——把 PC 设为开关字，并显示数据
    X   EXAMINE NEXT——PC+1，并显示数据
    p   DEPOSIT——把数据字节写入 PC 处内存
    P   DEPOSIT NEXT——PC+1 后写入
    R   RESET——PC = 0
    C   CLR——清空内存与寄存器
    0-9, a-f   翻转某个开关位（仅控制台）
    l   载入示例程序
    i   信息——寄存器 以及 围绕 PC、SP 的内存转储（hex + ASCII）
    j   设置信息
    u/U 日志开关
    v/V VT100 虚拟前面板开关
    t/T 终端转义码开关
    h   帮助
    L   从串口载入 Intel hex
    o/O LED 输出使能/禁用（旧功能；对 PCA9555 面板无效）
    w/W USB 串行输出开关
    y/Y, B, D   辅助 Serial1（UART）控制——可选

i 信息行会把数据字节按 2-3-3 位分组 + 十六进制 显示，例如 “00 111 010  (3A)”。
x/X/p/P 的数据显示在与地址**同一行、靠右对齐**的位置。

SD 卡 / MP3 / 时钟相关命令（F, m, M, n, J, Q, q……）仅是菜单占位，**不执行任何
操作**（见未实装项）。

## 6. 未实装项

- 无 TF/SD 卡支持。SD 读写函数均为空桩；setupSdCard() 始终返回 false。程序的
  载入/保存只能通过串口控制台或示例程序。
- 无 MP3 播放器、无实时时钟（亦为空桩）。
- PCA9555 必须确实是 PCA9555（PCF8575 不能直接替代）。
- 已通过编译及“无面板”代码路径验证；实体 I2C/LED/开关 的行为请在真实硬件上确认。

## 7. 文件

    Altair2040/
      Altair2040.ino      主程序（由 Altair101a.ino 移植）
      cpuIntel8080.*      Intel 8080 模拟器内核（含 RP2040 分支）
      AltairSamples.*     示例程序（Kill the Bit、4K BASIC 等）
      Altair101b.h        共用模块头文件（已移除 IRremote/MP3/clock/SD）
      Panel9555.*         PCA9555 前面板驱动
      panel_config.h       *** 改面板接线时只需编辑本文件 ***
      build.sh            编译脚本 -> build/Altair2040.ino.uf2

## 8. Git

本地仓库，已提交移植后的工程。编译中间产物 .build/ 已被 git 忽略；可直接烧录的
build/Altair2040.ino.uf2 纳入版本管理。
