---
name: embedded-protocols
description: 嵌入式通信协议开发指南，覆盖 UART/USART、SPI、I2C/TWI、CAN、1-Wire、Modbus（RTU/ASCII/TCP）、RS-485、USB CDC、以太网/TCP-UDP、MQTT 等协议的原理、时序、代码实现、调试方法与常见问题排查。用户提及串口、SPI、I2C、CAN、Modbus、RS485、通信协议、总线时使用。
---

# 嵌入式通信协议开发指南

## 概述

本 Skill 覆盖嵌入式系统中最常用的有线和无线通信协议，提供原理说明、代码模板、调试技巧和问题排查。每个协议包含：物理层特性、时序图要点、主从/收发代码、错误处理和调试方法。

## 协议选型速查

| 协议 | 线数 | 速率 | 距离 | 主从 | 典型用途 |
|------|------|------|------|------|----------|
| UART | 2 (TX/RX) | 最高 ~10Mbps | <15m | 点对点 | 调试串口、GPS、蓝牙模块 |
| SPI | 4 (SCK/MOSI/MISO/CS) | 最高 ~100Mbps | <1m | 一主多从 | Flash、显示屏、ADC、传感器 |
| I2C | 2 (SDA/SCL) | 100k/400k/1M/3.4M | <1m | 一主多从 | 传感器、EEPROM、OLED |
| CAN | 2 (CANH/CANL) | 最高 1Mbps | <1km | 多主 | 汽车、工业控制 |
| 1-Wire | 1 (DQ) | 16kbps | <100m | 一主多从 | DS18B20 温度传感器 |
| RS-485 | 2 (A/B) | 最高 10Mbps | <1200m | 多节点 | 工业总线、Modbus RTU |
| Modbus RTU | 基于 RS-485/UART | 取决于物理层 | 同 RS-485 | 一主多从 | 工业设备通信 |
| Ethernet | 8 (RJ45) | 10/100/1000Mbps | <100m | - | 物联网网关、TCP/UDP |
| USB CDC | 4 (USB) | 12/480Mbps | <5m | 主从 | 虚拟串口、调试 |

---

## UART / USART

### 原理

异步串行通信，无需时钟线，双方约定波特率、数据位、停止位、校验位。起始位（低电平）+ 数据位（LSB 先发）+ 校验位（可选）+ 停止位（高电平）。

### 关键参数

```
波特率：9600 / 19200 / 38400 / 57600 / 115200 / 230400 / 460800 / 921600
数据位：7 / 8 / 9
停止位：1 / 1.5 / 2
校验：None / Even / Odd / Mark / Space
流控：None / RTS-CTS（硬件）/ XON-XOFF（软件）
```

### STM32 HAL 实现

```c
// 初始化（CubeMX 生成）
huart1.Instance = USART1;
huart1.Init.BaudRate = 115200;
huart1.Init.WordLength = UART_WORDLENGTH_8B;
huart1.Init.StopBits = UART_STOPBITS_1;
huart1.Init.Parity = UART_PARITY_NONE;
huart1.Init.Mode = UART_MODE_TX_RX;
huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
huart1.Init.OverSampling = UART_OVERSAMPLING_16;
HAL_UART_Init(&huart1);

// 阻塞发送
HAL_UART_Transmit(&huart1, (uint8_t*)"OK\r\n", 4, 100);

// 中断接收（单字节，推荐用于命令解析）
uint8_t rx_byte;
HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // 处理 rx_byte（如加入环形缓冲区）
        ringbuf_put(&rb, rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // 重新启动
    }
}

// DMA 接收（空闲中断 + DMA，高效接收不定长数据）
uint8_t dma_rx_buf[256];
HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dma_rx_buf, 256);

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART1) {
        // dma_rx_buf[0..Size-1] 是接收到的数据
        process_data(dma_rx_buf, Size);
    }
}
```

### Arduino 实现

```cpp
Serial.begin(115200, SERIAL_8N1);  // 8数据位，无校验，1停止位

// 发送
Serial.print("Temp: ");
Serial.println(25.5);
Serial.write(0x55);  // 原始字节

// 接收
if (Serial.available()) {
    char c = Serial.read();
    String line = Serial.readStringUntil('\n');
}

// 多串口（ESP32）
Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17
```

### 环形缓冲区实现

```c
#define RB_SIZE 256
typedef struct {
    uint8_t buf[RB_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} ringbuf_t;

void ringbuf_put(ringbuf_t *rb, uint8_t b) {
    uint16_t next = (rb->head + 1) % RB_SIZE;
    if (next != rb->tail) {
        rb->buf[rb->head] = b;
        rb->head = next;
    }
}

int ringbuf_get(ringbuf_t *rb) {
    if (rb->head == rb->tail) return -1;
    uint8_t b = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RB_SIZE;
    return b;
}
```

### 调试

- **串口助手**：SSCOM、Putty、Tera Term、PlatformIO Serial Monitor
- **逻辑分析仪**：Saleae、DSLogic，设置 UART 协议解码
- **常见问题**：
  - 乱码 → 波特率不匹配、时钟配置错误、共地问题
  - 丢数据 → 阻塞发送太久、接收缓冲区溢出
  - 接收不到 → TX/RX 接反、未共地、电平不匹配（3.3V vs 5V）

---

## SPI

### 原理

同步串行总线，4 根线：SCLK（时钟）、MOSI（主出从入）、MISO（主入从出）、CS/SS（片选，低电平有效）。一主多从，每个从机单独 CS。全双工。

### 四种模式（CPOL/CPHA）

| 模式 | CPOL | CPHA | 空闲电平 | 采样边沿 |
|------|------|------|----------|----------|
| 0 | 0 | 0 | 低 | 上升沿 |
| 1 | 0 | 1 | 低 | 下降沿 |
| 2 | 1 | 0 | 高 | 下降沿 |
| 3 | 1 | 1 | 高 | 上升沿 |

**必须与从机手册一致！** 最常用 Mode 0。

### STM32 HAL 实现

```c
// 初始化（CubeMX 生成）
hspi1.Instance = SPI1;
hspi1.Init.Mode = SPI_MODE_MASTER;
hspi1.Init.Direction = SPI_DIRECTION_2LINES;
hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;      // CPOL=0
hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;            // CPHA=0
hspi1.Init.NSS = SPI_NSS_SOFT;                     // 软件 CS
hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  // 主频/8
hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
HAL_SPI_Init(&hspi1);

// 全双工传输
uint8_t tx[4] = {0x01, 0x02, 0x03, 0x04};
uint8_t rx[4];
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // CS 拉低
HAL_SPI_TransmitReceive(&hspi1, tx, rx, 4, 100);
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);    // CS 拉高

// 只发送
HAL_SPI_Transmit(&hspi1, tx, 4, 100);

// 只接收
HAL_SPI_Receive(&hspi1, rx, 4, 100);

// DMA 方式
HAL_SPI_TransmitReceive_DMA(&hspi1, tx, rx, 4);
```

### Arduino 实现

```cpp
#include <SPI.h>
const int CS_PIN = 10;

void setup() {
    SPI.begin();
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
}

uint8_t spiTransfer(uint8_t data) {
    digitalWrite(CS_PIN, LOW);
    uint8_t result = SPI.transfer(data);
    digitalWrite(CS_PIN, HIGH);
    return result;
}

// 设置模式和速率
SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
// ... 传输 ...
SPI.endTransaction();
```

### 读寄存器模板（大多数 SPI 传感器）

```c
// 读寄存器：先发读命令（最高位=1）+ 寄存器地址，再发 dummy 读取
uint8_t spi_read_reg(uint8_t reg) {
    uint8_t tx[2] = {reg | 0x80, 0x00};  // 读标志
    uint8_t rx[2];
    CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);
    CS_HIGH();
    return rx[1];
}

// 写寄存器
void spi_write_reg(uint8_t reg, uint8_t val) {
    uint8_t tx[2] = {reg & 0x7F, val};  // 写标志
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, tx, 2, 100);
    CS_HIGH();
}
```

### 调试

- 逻辑分析仪解码 SPI，检查 CPOL/CPHA、CS 时序
- 常见问题：
  - 数据全 0xFF → MISO 未接或从机未响应
  - 数据错位 → 模式不匹配或 CS 时序错误
  - 偶发错误 → 速率过高、走线太长、CS 拉低后未加延时

---

## I2C / TWI

### 原理

两线制同步总线：SDA（数据）、SCL（时钟），均需上拉电阻（典型 4.7kΩ）。一主多从，7 位地址（也有 10 位），半双工。起始条件（SCL 高时 SDA 下降沿）+ 地址+读写位 + ACK + 数据 + 停止条件（SCL 高时 SDA 上升沿）。

### 标准速率

- 标准模式：100 kbps
- 快速模式：400 kbps
- 快速模式+：1 Mbps
- 高速模式：3.4 Mbps

### STM32 HAL 实现

```c
// 初始化（CubeMX 生成）
hi2c1.Instance = I2C1;
hi2c1.Init.ClockSpeed = 400000;
hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
hi2c1.Init.OwnAddress1 = 0;
hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
HAL_I2C_Init(&hi2c1);

// 注意：HAL 中设备地址需左移一位（8 位格式：addr<<1 | R/W）
#define DEV_ADDR 0x68  // 7 位地址

// 写寄存器（8 位寄存器地址）
HAL_I2C_Mem_Write(&hi2c1, DEV_ADDR << 1, 0x75, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

// 读寄存器
uint8_t data;
HAL_I2C_Mem_Read(&hi2c1, DEV_ADDR << 1, 0x75, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);

// 多字节读
uint8_t buf[6];
HAL_I2C_Mem_Read(&hi2c1, DEV_ADDR << 1, 0x3B, I2C_MEMADD_SIZE_8BIT, buf, 6, 100);

// 16 位寄存器地址（如 EEPROM）
HAL_I2C_Mem_Write(&hi2c1, 0x50 << 1, 0x0010, I2C_MEMADD_SIZE_16BIT, data, 4, 100);

// 扫描 I2C 总线（调试用）
for (uint8_t addr = 1; addr < 127; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
        printf("Found device at 0x%02X\r\n", addr);
    }
}
```

### Arduino 实现

```cpp
#include <Wire.h>

void setup() {
    Wire.begin();           // 主机模式
    Wire.setClock(400000);  // 400kHz
}

// 写寄存器
void writeReg(uint8_t dev, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(dev);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

// 读寄存器
uint8_t readReg(uint8_t dev, uint8_t reg) {
    Wire.beginTransmission(dev);
    Wire.write(reg);
    Wire.endTransmission(false);  // 不发送停止条件
    Wire.requestFrom(dev, (uint8_t)1);
    return Wire.read();
}

// 扫描
for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
        Serial.printf("I2C device found at 0x%02X\n", addr);
    }
}
```

### 软件 I2C（位操作，任意引脚）

```c
#define SDA_PIN GPIO_PIN_7
#define SCL_PIN GPIO_PIN_6
#define I2C_PORT GPIOB

void i2c_start(void) {
    SDA_HIGH(); SCL_HIGH(); delay_us(5);
    SDA_LOW();  delay_us(5);
    SCL_LOW();  delay_us(5);
}

void i2c_stop(void) {
    SDA_LOW();  SCL_HIGH(); delay_us(5);
    SDA_HIGH(); delay_us(5);
}

uint8_t i2c_write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        if (byte & 0x80) SDA_HIGH(); else SDA_LOW();
        byte <<= 1;
        SCL_HIGH(); delay_us(5);
        SCL_LOW();  delay_us(5);
    }
    SDA_HIGH();  // 释放 SDA 等待 ACK
    SCL_HIGH(); delay_us(5);
    uint8_t ack = HAL_GPIO_ReadPin(I2C_PORT, SDA_PIN);  // 0=ACK
    SCL_LOW();
    return ack;
}
```

### 调试

- 逻辑分析仪解码 I2C，检查 ACK、地址、数据
- 常见问题：
  - 一直 BUSY → SDA 被从机拉低（从机崩溃），需软件复位或重新上电
  - NACK → 地址错误、设备未上电、上拉电阻缺失
  - 通信不稳定 → 上拉电阻值不对（长线用 2.2kΩ）、速率过高
  - HAL_I2C 卡死 → 使能 I2C 超时中断，或在超时后重新初始化 I2C

---

## CAN

### 原理

差分串行总线，CANH/CANL，抗干扰强，多主架构，广播式通信，非破坏性仲裁（标识符越小优先级越高）。数据帧：SOF + 仲裁段（ID+RTR+IDE）+ 控制段（DLC）+ 数据段（0-8 字节，CAN FD 最多 64 字节）+ CRC + ACK + EOF。

### STM32 HAL 实现（bxCAN）

```c
// 初始化（CubeMX 生成）
hcan1.Instance = CAN1;
hcan1.Init.Prescaler = 6;                  // 波特率分频
hcan1.Init.Mode = CAN_MODE_NORMAL;          // 正常模式；回环模式用 CAN_MODE_LOOPBACK
hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;        // 位时序：1+13+2=16TQ
hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
hcan1.Init.TimeTriggeredMode = DISABLE;
hcan1.Init.AutoBusOff = ENABLE;             // 自动总线恢复
hcan1.Init.AutoWakeUp = DISABLE;
hcan1.Init.AutoRetransmission = ENABLE;
hcan1.Init.ReceiveFifoLocked = DISABLE;
hcan1.Init.TransmitFifoPriority = DISABLE;
HAL_CAN_Init(&hcan1);

// 波特率计算：APB1时钟 / (Prescaler * (1+BS1+BS2))
// 例：42MHz / (6 * 16) = 437.5 kbps（接近 500k，需调整）

// 配置过滤器（接收哪些 ID）
CAN_FilterTypeDef sFilterConfig;
sFilterConfig.FilterBank = 0;
sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;  // 掩码模式
sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
sFilterConfig.FilterIdHigh = 0x0000;       // 接收所有 ID
sFilterConfig.FilterIdLow = 0x0000;
sFilterConfig.FilterMaskIdHigh = 0x0000;
sFilterConfig.FilterMaskIdLow = 0x0000;
sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
sFilterConfig.FilterActivation = ENABLE;
sFilterConfig.SlaveStartFilterBank = 14;
HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

// 启动 CAN
HAL_CAN_Start(&hcan1);
HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);  // 使能接收中断

// 发送
CAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
uint32_t TxMailbox;

TxHeader.StdId = 0x123;       // 标准 ID（11 位）
TxHeader.ExtId = 0;            // 扩展 ID（29 位，IDE=1 时有效）
TxHeader.IDE = CAN_ID_STD;     // 标准帧
TxHeader.RTR = CAN_RTR_DATA;   // 数据帧
TxHeader.DLC = 8;               // 数据长度
TxHeader.TransmitGlobalTime = DISABLE;

HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);

// 接收中断回调
CAN_RxHeaderTypeDef RxHeader;
uint8_t RxData[8];

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData);
    // RxHeader.StdId / ExtId：ID
    // RxHeader.DLC：数据长度
    // RxData[0..DLC-1]：数据
}
```

### 调试

- **CAN 分析仪**：PCAN-USB、CANable、ZLG USBCAN
- **回环测试**：`CAN_MODE_LOOPBACK` 自发自收，验证软件配置
- 常见问题：
  - 发送失败（邮箱满）→ 总线无其他节点应答，需至少两个节点或启用回环
  - 进入 Bus-Off → 错误计数器超限，检查终端电阻（120Ω）、接线、波特率
  - 接收不到 → 过滤器配置错误、波特率不匹配
  - 数据错误 → 终端电阻缺失、CANH/CANL 接反

---

## RS-485 / Modbus RTU

### RS-485 物理层

- 差分信号 A/B（A 负，B 正），半双工
- 需方向控制引脚（DE/RE），DE 高=发送，RE 低=接收
- 总线两端各接 120Ω 终端电阻
- 最多 32 个节点（标准），中继器可扩展

### RS-485 收发（STM32）

```c
#define RS485_DE_PORT GPIOA
#define RS485_DE_PIN  GPIO_PIN_8

void rs485_send(uint8_t *data, uint16_t len) {
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);  // 发送模式
    HAL_UART_Transmit(&huart2, data, len, 100);
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET);  // 等待发送完成
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);  // 切回接收
}
```

### Modbus RTU 帧格式

```
[从机地址 1B] [功能码 1B] [数据 N B] [CRC16 2B 低字节在前]
```

功能码：
- 0x01：读线圈（DO）
- 0x02：读离散输入（DI）
- 0x03：读保持寄存器
- 0x04：读输入寄存器
- 0x05：写单个线圈
- 0x06：写单个寄存器
- 0x0F：写多个线圈
- 0x10：写多个寄存器

### Modbus RTU 主机实现（读保持寄存器）

```c
// CRC16-Modbus 计算
uint16_t modbus_crc16(uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

// 读保持寄存器（功能码 0x03）
// slave: 从机地址, reg: 起始寄存器, count: 寄存器数量
HAL_StatusTypeDef modbus_read_holding(uint8_t slave, uint16_t reg, uint16_t count, uint16_t *result) {
    uint8_t tx[8] = {slave, 0x03, (reg >> 8) & 0xFF, reg & 0xFF,
                      (count >> 8) & 0xFF, count & 0xFF, 0, 0};
    uint16_t crc = modbus_crc16(tx, 6);
    tx[6] = crc & 0xFF;        // CRC 低字节
    tx[7] = (crc >> 8) & 0xFF; // CRC 高字节

    rs485_send(tx, 8);

    // 接收响应：[addr][0x03][byte_count][data...][crc_lo][crc_hi]
    uint8_t rx[64];
    HAL_UART_Receive(&huart2, rx, 5 + count * 2, 1000);  // 超时 1s

    // 校验 CRC
    uint16_t recv_crc = modbus_crc16(rx, 3 + count * 2);
    if (recv_crc != (rx[3 + count * 2] | (rx[4 + count * 2] << 8))) {
        return HAL_ERROR;
    }

    // 解析寄存器值（大端）
    for (uint16_t i = 0; i < count; i++) {
        result[i] = (rx[3 + i * 2] << 8) | rx[4 + i * 2];
    }
    return HAL_OK;
}
```

### Modbus TCP

基于以太网 TCP，端口 502，帧格式：
```
[事务标识符 2B] [协议标识符 2B=0] [长度 2B] [单元标识符 1B] [功能码 1B] [数据...]
```
使用标准 TCP Socket 通信，无需 CRC。

### 调试

- **Modbus 调试工具**：Modbus Poll（主机）、Modbus Slave（从机）、QModMaster
- **USB 转 RS485 模块**：CH340 芯片，接 A/B 线
- 常见问题：
  - 无响应 → 从机地址错误、波特率不匹配、A/B 接反、未共地
  - CRC 错误 → 波特率误差大、干扰、终端电阻缺失
  - 偶发丢包 → 未等待发送完成就切方向、DE 引脚时序不对

---

## 1-Wire

### 原理

单总线，一根 DQ 线 + 地，寄生供电可选。每个设备有唯一 64 位 ROM 地址。时序严格，需微秒级延时。

### DS18B20 温度读取（STM32 位操作）

```c
#define ONEWIRE_PORT GPIOA
#define ONEWIRE_PIN  GPIO_PIN_0

// 复位脉冲
uint8_t onewire_reset(void) {
    ONEWIRE_OUTPUT();
    ONEWIRE_LOW();
    delay_us(480);
    ONEWIRE_INPUT();  // 释放总线（上拉）
    delay_us(70);
    uint8_t present = !ONEWIRE_READ();  // 存在脉冲（低电平）
    delay_us(410);
    return present;
}

// 写一位
void onewire_write_bit(uint8_t bit) {
    ONEWIRE_OUTPUT();
    ONEWIRE_LOW();
    delay_us(bit ? 6 : 60);
    if (bit) ONEWIRE_INPUT();  // 写1：释放总线
    delay_us(bit ? 64 : 10);
    ONEWIRE_INPUT();
}

// 读一位
uint8_t onewire_read_bit(void) {
    uint8_t bit;
    ONEWIRE_OUTPUT();
    ONEWIRE_LOW();
    delay_us(6);
    ONEWIRE_INPUT();
    delay_us(9);
    bit = ONEWIRE_READ();
    delay_us(55);
    return bit;
}

// 写字节
void onewire_write_byte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        onewire_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

// 读字节
uint8_t onewire_read_byte(void) {
    uint8_t byte = 0;
    for (uint8_t i = 0; i < 8; i++) {
        byte >>= 1;
        if (onewire_read_bit()) byte |= 0x80;
    }
    return byte;
}

// DS18B20 读温度
float ds18b20_read_temp(void) {
    if (!onewire_reset()) return -999.0f;  // 设备不存在
    onewire_write_byte(0xCC);  // Skip ROM
    onewire_write_byte(0x44);  // Convert T
    HAL_Delay(750);             // 等待转换（12位精度最长 750ms）

    onewire_reset();
    onewire_write_byte(0xCC);  // Skip ROM
    onewire_write_byte(0xBE);  // Read Scratchpad

    uint8_t temp_l = onewire_read_byte();
    uint8_t temp_h = onewire_read_byte();
    int16_t raw = (temp_h << 8) | temp_l;
    return raw / 16.0f;  // 12 位精度，0.0625°C/LSB
}
```

---

## 以太网 / TCP / UDP

### STM32 + LwIP（CubeMX 集成）

```c
// CubeMX 中启用 ETH + LwIP，选择 RMII 接口
// 生成后在 netconf.c / lwip.c 中

// TCP 客户端示例
#include "lwip/sockets.h"

int tcp_client_test(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    inet_pton(AF_INET, "192.168.1.100", &server.sin_addr);

    connect(sock, (struct sockaddr*)&server, sizeof(server));

    char *msg = "Hello TCP";
    send(sock, msg, strlen(msg), 0);

    char buf[128];
    int len = recv(sock, buf, sizeof(buf) - 1, 0);
    buf[len] = '\0';

    closesocket(sock);
    return 0;
}

// UDP 示例
int udp_test(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_port = htons(5000);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sock, (struct sockaddr*)&local, sizeof(local));

    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    char buf[128];
    int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&remote, &remote_len);

    sendto(sock, "ACK", 3, 0, (struct sockaddr*)&remote, remote_len);
    closesocket(sock);
    return 0;
}
```

### ESP32 WiFi + TCP

```cpp
#include <WiFi.h>
WiFiClient client;

void setup() {
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    if (client.connect("192.168.1.100", 8080)) {
        client.println("GET / HTTP/1.1");
        client.println("Host: 192.168.1.100");
        client.println();
    }
}

void loop() {
    while (client.available()) {
        Serial.write(client.read());
    }
}
```

---

## MQTT

### ESP32 + PubSubClient

```cpp
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("Message [%s]: ", topic);
    for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
    Serial.println();
}

void reconnect() {
    while (!client.connected()) {
        if (client.connect("ESP32Client", "user", "pass")) {
            client.subscribe("home/room1/light");
        } else {
            delay(5000);
        }
    }
}

void setup() {
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    client.setServer("broker.example.com", 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();
    client.publish("home/room1/temp", "25.5");
    delay(1000);
}
```

### MQTT QoS 级别

| QoS | 含义 | 适用场景 |
|-----|------|----------|
| 0 | 至多一次（发完即弃） | 传感器高频数据，丢一两个无所谓 |
| 1 | 至少一次（需确认，可能重复） | 控制指令，必须到达但可重复 |
| 2 | 恰好一次（四次握手） | 关键交易、计费数据 |

---

## 通用调试方法

### 逻辑分析仪使用

1. 连接 GND 共地，通道接对应信号线
2. 设置采样率（至少 10 倍于信号速率）
3. 选择协议解码器（UART/SPI/I2C/CAN）
4. 设置协议参数（波特率、CPOL/CPHA、位序等）
5. 触发捕获，查看解码结果

### 常用工具

| 工具 | 用途 |
|------|------|
| Saleae Logic / DSLogic | 逻辑分析仪软件 |
| SSCOM / Putty | 串口调试 |
| CANable / PCAN-USB | CAN 总线分析 |
| Modbus Poll / Slave | Modbus 调试 |
| Wireshark | 网络抓包（TCP/UDP/MQTT） |
| MQTT.fx / MQTTX | MQTT 客户端调试 |
| oscilloscope | 示波器，查看信号质量、上升沿 |

### 通用排查步骤

1. **物理层**：接线是否正确？是否共地？电平是否匹配（3.3V/5V）？上拉/终端电阻？
2. **参数**：波特率/速率、模式（CPOL/CPHA）、地址、ID 是否双方一致？
3. **时序**：用逻辑分析仪抓包，对比协议规范
4. **软件**：初始化是否成功？中断是否使能？缓冲区是否溢出？
5. **环境**：干扰源（电机、开关电源）？屏蔽线？接地？

## 参考资源

- [1-Wire 协议规范](https://www.analog.com/en/technical-articles/guide-to-1-wire-communication.html)
- [Modbus 协议规范](https://modbus.org/specs.php)
- [CAN 总线入门](https://www.csselectronics.com/pages/can-bus-simple-intro-tutorial)
- [LwIP 官方文档](https://www.nongnu.org/lwip/2_1_x/index.html)
