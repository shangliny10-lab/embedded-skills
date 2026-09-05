---
name: embedded-stm32-dev
description: STM32 微控制器开发全流程指南，覆盖 CubeMX 配置、HAL/LL 库编程、Keil/STM32CubeIDE 工程、调试烧录、常见外设驱动（GPIO/UART/SPI/I2C/TIM/ADC/DMA）、中断与低功耗。用户提及 STM32、HAL 库、CubeMX、Keil 工程、STM32 外设、STM32 调试时使用。
---

# STM32 开发指南

## 概述

本 Skill 面向 STM32 系列微控制器（F0/F1/F4/F7/H7/G0/G4/L4/L5/U5 等）的嵌入式开发，提供从工程创建到调试部署的完整工作流。优先使用 STM32CubeMX 生成初始化代码 + HAL 库，性能敏感场景可混用 LL 库或直接操作寄存器。

## 核心规则

- **工程生成**：始终通过 STM32CubeMX 配置引脚、时钟树、外设并生成代码，不要手动新建 `.ioc` 工程。
- **用户代码区**：所有自定义代码必须写在 `/* USER CODE BEGIN x */` 和 `/* USER CODE END x */` 之间，否则重新生成 CubeMX 时会被覆盖。
- **HAL 优先**：默认使用 HAL 库 API；对时序要求极高（如高频 PWM、精确延时）的场景使用 LL 库或寄存器直写，并在注释中标注原因。
- **时钟配置**：使用 CubeMX 时钟树图形界面配置，HSE/HSI 选择、PLL 参数、总线分频必须与实际硬件匹配。
- **中断优先级**：使用 `HAL_NVIC_SetPriority()` 配置，注意 FreeRTOS 下中断优先级必须高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 才能调用安全 API。
- **烧录工具**：优先使用 ST-Link（SWD 接口），备选 J-Link、串口 ISP（USART1 BOOT0）。

## 快速开始

### 1. CubeMX 工程创建流程

1. 打开 STM32CubeMX → `New Project` → 选择具体芯片型号（如 STM32F407VGT6）
2. **Pinout & Configuration** 标签页：
   - 配置调试接口：SYS → Debug → Serial Wire（否则烧录一次后 SWD 失效）
   - 配置时钟源：RCC → High Speed Clock (HSE) → Crystal/Ceramic Resonator
   - 按需启用外设（USART1、SPI1、I2C1 等）并分配引脚
3. **Clock Configuration** 标签页：配置系统时钟（如 F407 配 168MHz）
4. **Project Manager** 标签页：
   - Project Name / Location
   - Toolchain/IDE：MDK-ARM（Keil）或 STM32CubeIDE
   - 勾选 "Generate peripheral initialization as a pair of '.c/.h' files per peripheral"
5. 点击 `GENERATE CODE`

### 2. 工程目录结构

```
Project/
├── Core/
│   ├── Inc/           # main.h, stm32f4xx_hal_conf.h, 外设头文件
│   ├── Src/           # main.c, stm32f4xx_hal_msp.c, 外设源文件
│   └── Startup/       # 启动汇编文件
├── Drivers/
│   ├── STM32F4xx_HAL_Driver/  # HAL 库源码
│   └── CMSIS/                  # 内核与设备头文件
├── MDK-ARM/           # Keil 工程文件（.uvprojx）
└── Project.ioc        # CubeMX 配置文件
```

## 常用外设 HAL API 速查

### GPIO

```c
// 初始化（CubeMX 生成，通常在 gpio.c）
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Pin = GPIO_PIN_13;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // 推挽输出
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

// 操作
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    // 高电平
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  // 低电平
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);                  // 翻转
GPIO_PinState state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

// 外部中断
GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;  // 上升沿触发
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(EXTI0_IRQn);
// 中断回调（写在 USER CODE 区）
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) { /* 处理 */ }
}
```

### UART / USART

```c
// 发送（阻塞）
HAL_UART_Transmit(&huart1, (uint8_t*)"Hello\r\n", 7, 100);

// 接收（阻塞）
uint8_t buf[32];
HAL_UART_Receive(&huart1, buf, 10, 1000);  // 超时 1s

// 中断方式发送
HAL_UART_Transmit_IT(&huart1, tx_buf, len);

// 中断方式接收（推荐：先启动接收，在回调中处理）
HAL_UART_Receive_IT(&huart1, rx_byte, 1);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // 处理 rx_byte
        HAL_UART_Receive_IT(&huart1, rx_byte, 1);  // 重新启动接收
    }
}

// DMA 方式（大数据量）
HAL_UART_Transmit_DMA(&huart1, tx_buf, len);
HAL_UART_Receive_DMA(&huart1, rx_buf, len);
```

### SPI

```c
// 主模式发送/接收（全双工）
uint8_t tx[2] = {0x01, 0x02};
uint8_t rx[2];
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // CS 拉低
HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);    // CS 拉高

// 只发送
HAL_SPI_Transmit(&hspi1, tx, 2, 100);

// DMA 方式
HAL_SPI_TransmitReceive_DMA(&hspi1, tx, rx, len);
```

### I2C

```c
// 写寄存器（设备地址 7 位，左移一位）
HAL_I2C_Mem_Write(&hi2c1, dev_addr << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 100);

// 读寄存器
HAL_I2C_Mem_Read(&hi2c1, dev_addr << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 100);

// 扫描 I2C 设备（调试用）
for (uint8_t addr = 1; addr < 127; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
        printf("Found device at 0x%02X\r\n", addr);
    }
}
```

### 定时器 TIM

```c
// PWM 输出
HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 500);  // 占空比

// 输入捕获
HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);

// 定时中断（毫秒级）
HAL_TIM_Base_Start_IT(&htim6);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM6) { /* 定时任务 */ }
}

// 微秒级延时（用 DWT 或定时器）
void delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}
```

### ADC

```c
// 单次转换（阻塞）
HAL_ADC_Start(&hadc1);
HAL_ADC_PollForConversion(&hadc1, 100);
uint32_t value = HAL_ADC_GetValue(&hadc1);
float voltage = value * 3.3f / 4096.0f;  // 12 位 ADC

// DMA 多通道连续采样
HAL_ADC_Start_DMA(&hadc1, adc_buffer, 16);
```

### DMA

```c
// 内存到外设（USART TX）
HAL_UART_Transmit_DMA(&huart1, tx_buf, len);

// 外存到内存（ADC）
HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buf, BUF_SIZE);

// 传输完成回调
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) { /* 发送完成 */ }
```

## 调试技巧

### 串口 printf 重定向

```c
// 在 main.c 的 USER CODE 区添加
#include <stdio.h>
int fputc(int ch, FILE *f) {
    HAL_UART_Transmit(&huart1, (uint8_t*)&ch, 1, 10);
    return ch;
}
// Keil 中需勾选 Use MicroLIB
```

### 常用调试命令

```c
// 打印变量
printf("ADC value: %lu, voltage: %.2fV\r\n", adc_val, voltage);

// 断言（调试版本）
assert_param(expression);

// 故障处理（写在 stm32f4xx_it.c 的 HardFault_Handler）
void HardFault_Handler(void) {
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);  // 故障闪烁
        HAL_Delay(200);
    }
}
```

### SWD 调试（Keil）

1. 魔术棒 → Debug → 选择 ST-Link Debugger → Settings
2. Flash Download → 勾选 Reset and Run
3. 点击 Debug（Ctrl+F5）进入调试
4. 常用：Run（F5）、Step Over（F10）、Step Into（F11）、Run to Cursor（Ctrl+F10）
5. Watch 窗口添加变量查看实时值

## 常见问题排查

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 烧录后无法再次连接 | 未启用 SWD 引脚或进入低功耗 | CubeMX 中 SYS→Debug→Serial Wire；用 ST-Link Utility 连接后擦除 |
| 串口乱码 | 波特率不匹配或时钟配置错误 | 检查 HSE_VALUE 定义、PLL 配置、USART_BRR |
| HAL_Delay 不准 | SysTick 中断被高优先级中断阻塞 | 提高 SysTick 优先级或减少临界区 |
| I2C 一直 BUSY | 上拉电阻缺失或从机拉低 SDA | 检查 4.7kΩ 上拉；软件复位 I2C |
| SPI 数据异常 | CPOL/CPHA 不匹配或 CS 时序错误 | 与从机手册核对模式；CS 拉低后加少量延时 |
| ADC 读数偏差大 | 参考电压不稳或未校准 | 使用 VREFINT 校准；增加采样时间 |
| 进入 HardFault | 栈溢出、空指针、数组越界 | 检查数组边界；增大栈大小（启动文件中） |

## 低功耗模式

```c
// STOP 模式（可被外部中断唤醒）
HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
// 唤醒后需重新配置系统时钟（HSI→PLL）

// STANDBY 模式（最低功耗，仅 RTC/唤醒引脚）
HAL_PWR_EnterSTANDBYMode();

// SLEEP 模式（WFI）
HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
```

## 参考资源

- [STM32CubeMX 官方下载](https://www.st.com/en/development-tools/stm32cubemx.html)
- [STM32 HAL 库文档](https://www.st.com/resource/en/user_manual/dm00105879.pdf)
- 各芯片 Reference Manual（RMxxxx）和 Datasheet
