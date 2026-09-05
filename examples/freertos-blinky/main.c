/*
 * FreeRTOS 多任务示例（STM32 HAL）
 * 功能：
 *   - Task1: LED 闪烁（500ms 周期）
 *   - Task2: 串口输出系统状态（1s 周期）
 *   - Task3: 按键检测（通过队列发送事件）
 *   - 队列：按键事件传递
 *   - 信号量：保护共享资源（UART 输出）
 *   - 互斥量：保护共享变量
 *
 * 基于 STM32F407 + FreeRTOS（CubeMX 集成，CMSIS_V2）
 */

#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* 句柄 --------------------------------------------------------------*/
osThreadId_t ledTaskHandle;
osThreadId_t statusTaskHandle;
osThreadId_t keyTaskHandle;
osMessageQueueId_t keyQueueHandle;
osSemaphoreId_t uartSemHandle;
osMutexId_t dataMutexHandle;

/* 共享数据 ----------------------------------------------------------*/
typedef struct {
    uint32_t led_toggles;
    uint32_t key_presses;
    float    temperature;
} SystemData;

SystemData sysData = {0};

/* 函数声明 ----------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void LEDTask(void *argument);
void StatusTask(void *argument);
void KeyTask(void *argument);
void safe_printf(const char *fmt, ...);

/* UART 句柄 ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* printf 重定向 -----------------------------------------------------*/
int fputc(int ch, FILE *f) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* main --------------------------------------------------------------*/
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    printf("\r\n=== FreeRTOS Multi-Task Demo ===\r\n");

    // 内核初始化（CubeMX 生成的 osKernelInitialize）
    osKernelInitialize();

    // 创建互斥量（保护共享数据）
    osMutexAttr_t mutex_attr = { .name = "dataMutex" };
    dataMutexHandle = osMutexNew(&mutex_attr);

    // 创建信号量（保护 UART 输出，二值信号量初始为1）
    osSemaphoreAttr_t sem_attr = { .name = "uartSem" };
    uartSemHandle = osSemaphoreNew(1, 1, &sem_attr);

    // 创建队列（按键事件，最多10个元素）
    osMessageQueueAttr_t queue_attr = { .name = "keyQueue" };
    keyQueueHandle = osMessageQueueNew(10, sizeof(uint32_t), &queue_attr);

    // 创建任务
    osThreadAttr_t led_attr = {
        .name = "ledTask",
        .stack_size = 256 * 4,  // 1024 字节
        .priority = osPriorityNormal,
    };
    ledTaskHandle = osThreadNew(LEDTask, NULL, &led_attr);

    osThreadAttr_t status_attr = {
        .name = "statusTask",
        .stack_size = 512 * 4,  // 2048 字节（printf 需要较大栈）
        .priority = osPriorityBelowNormal,
    };
    statusTaskHandle = osThreadNew(StatusTask, NULL, &status_attr);

    osThreadAttr_t key_attr = {
        .name = "keyTask",
        .stack_size = 128 * 4,
        .priority = osPriorityAboveNormal,
    };
    keyTaskHandle = osThreadNew(KeyTask, NULL, &key_attr);

    // 启动调度器
    osKernelStart();

    while (1) { /* 不会到达这里 */ }
}

/* LED 任务 ----------------------------------------------------------*/
void LEDTask(void *argument) {
    for (;;) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        // 更新共享数据（加锁）
        osMutexAcquire(dataMutexHandle, osWaitForever);
        sysData.led_toggles++;
        osMutexRelease(dataMutexHandle);

        osDelay(500);  // 500ms
    }
}

/* 状态输出任务 ------------------------------------------------------*/
void StatusTask(void *argument) {
    uint32_t tick_count = 0;
    for (;;) {
        tick_count++;

        // 读取共享数据（加锁）
        osMutexAcquire(dataMutexHandle, osWaitForever);
        uint32_t toggles = sysData.led_toggles;
        uint32_t presses = sysData.key_presses;
        osMutexRelease(dataMutexHandle);

        // 安全串口输出（信号量保护）
        safe_printf("[%5lu s] LED toggles: %lu, Key presses: %lu, Free heap: %u\r\n",
                    tick_count, toggles, presses,
                    (unsigned int)osThreadGetStackSpace(ledTaskHandle));

        osDelay(1000);  // 1s
    }
}

/* 按键检测任务 ------------------------------------------------------*/
void KeyTask(void *argument) {
    GPIO_PinState last_state = GPIO_PIN_RESET;
    uint32_t last_press_time = 0;

    for (;;) {
        GPIO_PinState state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

        // 上升沿检测 + 消抖
        if (state == GPIO_PIN_SET && last_state == GPIO_PIN_RESET) {
            if (HAL_GetTick() - last_press_time > 50) {  // 50ms 消抖
                last_press_time = HAL_GetTick();

                // 发送按键事件到队列
                uint32_t event = HAL_GetTick();
                osMessageQueuePut(keyQueueHandle, &event, 0, 0);

                // 更新共享数据
                osMutexAcquire(dataMutexHandle, osWaitForever);
                sysData.key_presses++;
                osMutexRelease(dataMutexHandle);

                safe_printf("[KEY] Pressed at %lu ms\r\n", event);
            }
        }
        last_state = state;

        osDelay(10);  // 10ms 扫描
    }
}

/* 安全 printf（信号量保护）-----------------------------------------*/
void safe_printf(const char *fmt, ...) {
    va_list args;
    osSemaphoreAcquire(uartSemHandle, osWaitForever);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    osSemaphoreRelease(uartSemHandle);
}

/* 系统时钟配置（168MHz）--------------------------------------------*/
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

/* USART1 初始化 -----------------------------------------------------*/
static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* GPIO 初始化 -------------------------------------------------------*/
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
