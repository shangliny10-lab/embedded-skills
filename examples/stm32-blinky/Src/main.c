/**
 * @file    main.c
 * @brief   STM32F407 LED 闪烁 + 串口输出示例
 * @board   STM32F407VGT6 (或任意 STM32，修改引脚即可)
 * @tools   STM32CubeMX + Keil MDK / STM32CubeIDE
 *
 * 功能：
 *   - LED (PC13) 每秒翻转一次
 *   - USART1 (PA9 TX, PA10 RX) 115200 波特率输出系统信息
 *   - 按键 (PA0) 外部中断，按下时翻转 LED
 *   - 微秒级延时（DWT）
 */

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

/* 私有变量 ----------------------------------------------------------*/
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim6;

/* 函数声明 ----------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM6_Init(void);
void DWT_Init(void);
void delay_us(uint32_t us);

/* printf 重定向（Keil 需勾选 Use MicroLIB）-------------------------*/
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/**
 * @brief  主函数
 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM6_Init();
    DWT_Init();

    printf("\r\n=== STM32F407 Blinky Demo ===\r\n");
    printf("System Clock: %lu MHz\r\n", SystemCoreClock / 1000000);
    printf("LED: PC13, UART1: 115200 8N1\r\n");
    printf("Press KEY (PA0) to toggle LED\r\n\n");

    uint32_t tick = 0;
    uint32_t counter = 0;

    while (1)
    {
        /* 非阻塞定时：每 500ms 翻转 LED */
        if (HAL_GetTick() - tick >= 500)
        {
            tick = HAL_GetTick();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            counter++;

            /* 每 2 秒（4次翻转）打印一次信息 */
            if (counter % 4 == 0)
            {
                printf("[%5lu s] LED toggled, uptime: %lu ms\r\n",
                       HAL_GetTick() / 1000, HAL_GetTick());
            }
        }

        /* 这里可以放其他非阻塞任务 */
    }
}

/**
 * @brief  DWT 初始化（用于微秒级延时）
 */
void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  微秒级延时（忙等，用 DWT 周期计数器）
 * @param  us 延时微秒数
 */
void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

/**
 * @brief  外部中断回调（按键 PA0）
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        /* 消抖：简单延时（中断中不宜太久，这里仅示例） */
        for (volatile int i = 0; i < 100000; i++);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            printf("[KEY] Button pressed, LED toggled\r\n");
        }
    }
}

/**
 * @brief  系统时钟配置（168MHz，HSE=8MHz）
 */
void SystemClock_Config(void)
{
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

/**
 * @brief  USART1 初始化（115200 8N1）
 */
static void MX_USART1_UART_Init(void)
{
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

/**
 * @brief  TIM6 初始化（可用于微秒级定时中断）
 */
static void MX_TIM6_Init(void)
{
    htim6.Instance = TIM6;
    htim6.Init.Prescaler = 84 - 1;   /* 84MHz / 84 = 1MHz */
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = 1000 - 1;     /* 1ms 周期 */
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim6);
}

/**
 * @brief  GPIO 初始化（LED PC13 推挽输出，KEY PA0 外部中断）
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* LED PC13 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* KEY PA0 外部中断（上升沿） */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}
