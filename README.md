# Embedded Skills 嵌入式开发技能集

一套面向嵌入式系统开发的 AI Agent Skills，覆盖从裸机开发到嵌入式 Linux 的完整技术栈。每个 Skill 包含原理说明、代码模板、调试技巧和常见问题排查，可直接用于 Doubao / 豆包等支持 Skill 的 AI 助手。

## 技能列表

| Skill | 描述 | 核心内容 |
|-------|------|----------|
| [embedded-stm32-dev](./embedded-stm32-dev) | STM32 微控制器开发 | CubeMX、HAL/LL 库、GPIO/UART/SPI/I2C/TIM/ADC/DMA、中断、低功耗、调试 |
| [embedded-arduino-dev](./embedded-arduino-dev) | Arduino / ESP32 开发 | Arduino IDE、PlatformIO、传感器驱动、WiFi/HTTP/MQTT、常用库 |
| [embedded-freertos](./embedded-freertos) | FreeRTOS 实时操作系统 | 任务管理、调度器、队列、信号量、互斥量、事件组、任务通知、软件定时器、内存管理 |
| [embedded-protocols](./embedded-protocols) | 嵌入式通信协议 | UART、SPI、I2C、CAN、1-Wire、RS-485、Modbus RTU/TCP、以太网/TCP-UDP、MQTT |
| [embedded-linux](./embedded-linux) | 嵌入式 Linux 开发 | 交叉编译、U-Boot、内核编译、设备树、Buildroot/Yocto、字符设备驱动、平台驱动、调试 |

## 安装使用

### 方式一：复制到 Skill 目录

将需要的 skill 文件夹复制到你的 AI 助手的 skills 目录下：

```bash
# 例如 Doubao / 豆包的 skills 目录
cp -r embedded-stm32-dev ~/.doubao/agent_mode/workspace/.skills/
```

### 方式二：作为参考文档

直接阅读每个 skill 下的 `SKILL.md`，作为嵌入式开发的速查手册和代码模板库。

## 目录结构

```
embedded-skills/
├── README.md
├── embedded-stm32-dev/
│   └── SKILL.md
├── embedded-arduino-dev/
│   └── SKILL.md
├── embedded-freertos/
│   └── SKILL.md
├── embedded-protocols/
│   └── SKILL.md
└── embedded-linux/
    └── SKILL.md
```

## 适用平台

- **MCU**：STM32 F0/F1/F4/F7/H7/G0/G4/L4/L5/U5、Arduino AVR、ESP32/ESP8266
- **RTOS**：FreeRTOS（STM32 CubeMX 集成、ESP-IDF/Arduino 内置）
- **Linux**：树莓派、全志 Allwinner、瑞芯微 Rockchip、NXP i.MX、TI AM335x 等 ARM 平台

## 贡献

欢迎提交 Issue 和 Pull Request 来补充更多嵌入式开发技能或修正内容。

## License

MIT License
