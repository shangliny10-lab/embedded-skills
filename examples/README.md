# 嵌入式示例项目

## 项目列表

### 1. stm32-blinky
STM32F407 LED 闪烁 + 串口输出 + 按键外部中断示例。
- 芯片：STM32F407VGT6（可移植到其他 STM32）
- 功能：LED 闪烁、串口 printf、按键中断、DWT 微秒延时
- 文件：`Src/main.c`、`Inc/main.h`
- 使用：用 STM32CubeMX 创建工程后替换 main.c

### 2. esp32-wifi-sensor
ESP32 WiFi 温湿度传感器 + MQTT 上传 + OLED 显示。
- 硬件：ESP32 DevKit + DHT22 + SSD1306 OLED
- 功能：WiFi 连接、DHT22 温湿度读取、MQTT 发布、OLED 显示、远程命令
- 文件：`esp32-wifi-sensor.ino`
- 依赖库：DHT、PubSubClient、Adafruit SSD1306、Adafruit GFX

### 3. freertos-blinky
FreeRTOS 多任务示例（STM32 HAL + CMSIS_V2）。
- 功能：LED 任务、状态输出任务、按键检测任务
- IPC：消息队列（按键事件）、信号量（UART 保护）、互斥量（共享数据保护）
- 文件：`main.c`
- 使用：CubeMX 启用 FreeRTOS 后替换 main.c
