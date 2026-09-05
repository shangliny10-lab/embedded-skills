---
name: embedded-freertos
description: FreeRTOS 实时操作系统开发指南，覆盖任务管理、调度器、队列、信号量、互斥量、事件组、任务通知、软件定时器、内存管理、中断安全 API、优先级反转与死锁排查。适用于 STM32/ESP32/ARM Cortex-M 等平台的 FreeRTOS 移植与应用开发。用户提及 FreeRTOS、RTOS、任务调度、队列、信号量、多任务时使用。
---

# FreeRTOS 开发指南

## 概述

FreeRTOS 是面向微控制器的轻量级实时操作系统，提供抢占式多任务调度、任务间通信（IPC）和同步原语。本 Skill 覆盖 FreeRTOS 核心 API、常见设计模式和调试技巧，适用于 STM32（CubeMX 集成）、ESP32（IDF/Arduino 内置）、ARM Cortex-M 等平台。

## 核心规则

- **任务永不返回**：任务函数必须是死循环 `while(1)`，绝不能 `return`；需要终止时调用 `vTaskDelete(NULL)`。
- **中断中只调用 FromISR API**：中断服务函数（ISR）中只能使用 `xQueueSendFromISR()`、`xSemaphoreGiveFromISR()` 等带 `FromISR` 后缀的函数，普通 API 会导致断言失败或系统崩溃。
- **优先级数值越大优先级越高**：FreeRTOS 中 `configMAX_PRIORITIES - 1` 是最高优先级，0 是最低（空闲任务优先级）。
- **栈大小单位是字（word），不是字节**：`xTaskCreate` 的 `usStackDepth` 参数单位为 `StackType_t`（32 位平台为 4 字节）。
- **临界区要短**：`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` 之间的代码必须极短（微秒级），否则会影响所有中断和任务调度。
- **避免在任务中死等**：用 `vTaskDelay()` 或阻塞 API 让 CPU 切换到其他任务，不要用空循环 `while(flag)` 忙等。

## 快速开始

### STM32 + CubeMX 集成

1. CubeMX → Pinout & Configuration → Middleware → FREERTOS → Interface 选择 `CMSIS_V2`
2. Configuration 标签页：
   - `configTOTAL_HEAP_SIZE`：堆大小（默认 15360 字节，按需调整）
   - `configMAX_PRIORITIES`：最大优先级数（默认 7）
   - `configUSE_PREEMPTION`：抢占式调度（默认开启）
3. Tasks and Queues 标签页：添加任务（设置名称、优先级、栈大小、入口函数）
4. 生成代码后，任务入口函数在 `freertos.c` 的 `USER CODE` 区实现

### ESP32（内置 FreeRTOS）

ESP32 的 Arduino/IDF 环境已内置 FreeRTOS，直接调用 API 即可：

```cpp
// ESP32 特有：指定任务运行在哪个核心
xTaskCreatePinnedToCore(taskFunc, "Task1", 2048, NULL, 1, NULL, 0);  // Core 0
xTaskCreatePinnedToCore(taskFunc, "Task2", 2048, NULL, 1, NULL, 1);  // Core 1
```

## 任务管理

### 创建任务

```c
// 动态创建（从堆分配栈和 TCB）
BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,     // 任务函数指针
    const char * const pcName,     // 任务名（调试用，最长 configMAX_TASK_NAME_LEN）
    configSTACK_DEPTH_TYPE usStackDepth,  // 栈大小（单位：字）
    void *pvParameters,             // 传入参数
    UBaseType_t uxPriority,         // 优先级（0 = 最低）
    TaskHandle_t *pxCreatedTask     // 任务句柄（可选，传 NULL）
);

// 示例
TaskHandle_t ledTaskHandle;
xTaskCreate(ledTask, "LED", 128, NULL, 2, &ledTaskHandle);

// 静态创建（用户提供内存，无堆碎片风险）
StaticTask_t xTaskBuffer;
StackType_t xStack[128];
xTaskCreateStatic(ledTask, "LED", 128, NULL, 2, xStack, &xTaskBuffer);
```

### 任务函数

```c
void ledTask(void *pvParameters) {
    // 初始化代码（运行一次）
    pinMode(LED_BUILTIN, OUTPUT);

    while (1) {  // 必须死循环
        digitalWrite(LED_BUILTIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));  // 阻塞 500ms，让出 CPU
        digitalWrite(LED_BUILTIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    // 不会执行到这里；如需终止：vTaskDelete(NULL);
}
```

### 任务控制 API

```c
vTaskDelay(pdMS_TO_TICKS(100));           // 相对延时（阻塞 100ms）
vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));  // 绝对延时（精确周期）

vTaskSuspend(ledTaskHandle);                // 挂起任务
vTaskResume(ledTaskHandle);                 // 恢复任务
vTaskDelete(NULL);                           // 删除自身
vTaskDelete(ledTaskHandle);                  // 删除其他任务

UBaseType_t prio = uxTaskPriorityGet(NULL); // 获取当前任务优先级
vTaskPrioritySet(ledTaskHandle, 3);          // 设置优先级

// 获取任务状态（调试用）
eTaskState state = eTaskGetState(ledTaskHandle);
// eRunning / eReady / eBlocked / eSuspended / eDeleted / eInvalid
```

### 绝对延时（精确周期任务）

```c
void periodicTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 10ms 周期

    while (1) {
        // 等待到下一个周期（从 xLastWakeTime 开始算，不累积误差）
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // 执行周期性任务（如采样、控制）
        readSensor();
    }
}
```

## 调度器

```c
vTaskStartScheduler();    // 启动调度器（STM32 CubeMX 自动调用，ESP32 已运行）
vTaskEndScheduler();      // 停止调度器（极少用）

// 调度策略（FreeRTOSConfig.h）
configUSE_PREEMPTION     = 1  // 抢占式（高优先级任务就绪立即执行）
configUSE_TIME_SLICING   = 1  // 时间片轮转（同优先级任务共享 CPU）
configTICK_RATE_HZ       = 1000  // 系统节拍频率（1ms 一拍）
```

## 队列（Queue）

队列是任务间数据传递的主要方式，线程安全，支持阻塞读写。

### 创建与使用

```c
// 创建队列（最多 10 个元素，每个元素是 int）
QueueHandle_t xQueue = xQueueCreate(10, sizeof(int));

// 发送（入队）
int value = 42;
xQueueSend(xQueue, &value, pdMS_TO_TICKS(100));  // 超时 100ms
xQueueSendToBack(xQueue, &value, 0);               // 队尾（默认）
xQueueSendToFront(xQueue, &value, 0);              // 队首（紧急数据）

// 接收（出队）
int received;
if (xQueueReceive(xQueue, &received, pdMS_TO_TICKS(500)) == pdTRUE) {
    // 成功接收到数据
}

// 查看（不删除队首元素）
xQueuePeek(xQueue, &received, 0);

// 队列状态
UBaseType_t count = uxQueueMessagesWaiting(xQueue);  // 当前元素数
UBaseType_t space = uxQueueSpacesAvailable(xQueue);   // 剩余空间
```

### 中断中使用队列

```c
// ISR 中发送
void USART1_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data = USART1->DR;
    xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // 触发上下文切换
}

// ISR 中接收
xQueueReceiveFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
```

### 队列集（Queue Set，多个队列等待）

```c
QueueSetHandle_t xQueueSet = xQueueCreateSet(10 + 5);
xQueueAddToSet(xQueue1, xQueueSet);
xQueueAddToSet(xQueue2, xQueueSet);

// 等待任意队列有数据
QueueSetMemberHandle_t xActivated = xQueueSelectFromSet(xQueueSet, pdMS_TO_TICKS(1000));
if (xActivated == xQueue1) {
    xQueueReceive(xQueue1, &data, 0);
}
```

## 信号量（Semaphore）

### 二值信号量（任务同步）

```c
SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();

// 任务中等待（获取）
if (xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // 获得信号量，执行受保护操作
}

// 任务/中断中释放（给出）
xSemaphoreGive(xSemaphore);

// ISR 中释放
xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
```

### 计数信号量（资源计数/多事件）

```c
// 创建：初始 0，最大 10
SemaphoreHandle_t xCountingSem = xSemaphoreCreateCounting(10, 0);

// 也可用于管理资源池（初始=最大）
SemaphoreHandle_t xPoolSem = xSemaphoreCreateCounting(5, 5);
xSemaphoreTake(xPoolSem, portMAX_DELAY);  // 取一个资源
// ... 使用资源 ...
xSemaphoreGive(xPoolSem);                   // 归还资源
```

## 互斥量（Mutex）

用于保护共享资源，支持优先级继承（防止优先级反转）。

```c
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// 加锁
if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // 访问共享资源（如全局变量、外设）
    sharedVariable++;
    // 解锁
    xSemaphoreGive(xMutex);
}

// 递归互斥量（同一任务可多次加锁）
SemaphoreHandle_t xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);  // 可重入
xSemaphoreGiveRecursive(xRecursiveMutex);
xSemaphoreGiveRecursive(xRecursiveMutex);  // 解锁两次才真正释放
```

**注意**：互斥量不能在 ISR 中使用（ISR 不能阻塞）。

## 事件组（Event Group）

用于一个任务等待多个事件中的任意一个或全部。

```c
EventGroupHandle_t xEventGroup = xEventGroupCreate();

#define EVENT_SENSOR_READY  (1 << 0)  // bit 0
#define EVENT_NETWORK_READY (1 << 1)  // bit 1
#define EVENT_ALL           (EVENT_SENSOR_READY | EVENT_NETWORK_READY)

// 设置事件位
xEventGroupSetBits(xEventGroup, EVENT_SENSOR_READY);

// 等待事件（等待任意一个，等待后清除）
EventBits_t bits = xEventGroupWaitBits(
    xEventGroup,
    EVENT_ALL,           // 等待的位
    pdTRUE,              // 等待后清除
    pdFALSE,             // pdFALSE=任意位满足即可；pdTRUE=全部满足
    pdMS_TO_TICKS(5000)
);
if (bits & EVENT_SENSOR_READY) { /* 传感器就绪 */ }

// ISR 中设置
xEventGroupSetBitsFromISR(xEventGroup, EVENT_NETWORK_READY, &xHigherPriorityTaskWoken);
```

## 任务通知（Task Notification）

比信号量/队列更快、更省内存的轻量级 IPC，每个任务自带一个 32 位通知值。

```c
// 发送通知（类似二值信号量）
xTaskNotifyGive(ledTaskHandle);

// 等待通知
ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));  // pdTRUE=清零（二值），pdFALSE=减一（计数）

// 带值通知
xTaskNotify(ledTaskHandle, 0x01, eSetBits);      // 设置位
xTaskNotify(ledTaskHandle, 42, eSetValueWithOverwrite);  // 覆盖值

// 等待带值通知
uint32_t notifiedValue;
xTaskNotifyWait(0, 0, &notifiedValue, pdMS_TO_TICKS(1000));

// ISR 中
vTaskNotifyGiveFromISR(ledTaskHandle, &xHigherPriorityTaskWoken);
```

## 软件定时器

```c
// 回调函数
void timerCallback(TimerHandle_t xTimer) {
    // 定时器到期执行（在定时器服务任务上下文，不要阻塞太久）
    Serial.println("Timer fired");
}

// 创建一次性定时器
TimerHandle_t xOneShotTimer = xTimerCreate(
    "OneShot", pdMS_TO_TICKS(5000), pdFALSE,  // pdFALSE=一次性
    (void*)0, timerCallback
);

// 创建周期定时器
TimerHandle_t xAutoReloadTimer = xTimerCreate(
    "AutoReload", pdMS_TO_TICKS(1000), pdTRUE,  // pdTRUE=周期
    (void*)0, timerCallback
);

xTimerStart(xAutoReloadTimer, 0);       // 启动
xTimerStop(xAutoReloadTimer, 0);         // 停止
xTimerReset(xAutoReloadTimer, 0);        // 重置（重新计时）
xTimerChangePeriod(xAutoReloadTimer, pdMS_TO_TICKS(2000), 0);  // 改周期
```

## 内存管理

### 堆方案（FreeRTOSConfig.h）

| 方案 | 文件 | 特点 |
|------|------|------|
| heap_1 | 只分配不释放 | 最简单，适合创建后不删除任务的系统 |
| heap_2 | 分配+释放，不合并 | 有碎片风险，适合固定大小分配 |
| heap_3 | 封装 malloc/free | 线程安全的标准库 malloc，需配置链接器堆 |
| heap_4 | 分配+释放+合并相邻块 | **推荐**，减少碎片 |
| heap_5 | 支持多内存区域 | 适合片内+片外 SRAM 混合使用 |

```c
// 配置（FreeRTOSConfig.h）
#define configAPPLICATION_ALLOCATED_HEAP 0
#define configTOTAL_HEAP_SIZE  ((size_t)(32 * 1024))  // 32KB 堆

// 运行时查询
size_t freeHeap = xPortGetFreeHeapSize();          // 当前空闲堆
size_t minEverFreeHeap = xPortGetMinimumEverFreeHeapSize();  // 历史最小空闲（评估峰值）
```

### 栈溢出检测

```c
// FreeRTOSConfig.h 开启
#define configCHECK_FOR_STACK_OVERFLOW 2  // 1=方法1（快），2=方法2（准）

// 实现钩子函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    Serial.printf("Stack overflow in task: %s\n", pcTaskName);
    while (1);  // 停机或复位
}
```

## 钩子函数（Hook Functions）

```c
// 空闲任务钩子（每次空闲任务运行时调用，可进入低功耗）
void vApplicationIdleHook(void) {
    __WFI();  // 等待中断，降低功耗
}

// 滴答钩子（每个 SysTick 调用，注意不能阻塞）
void vApplicationTickHook(void) { }

// 内存分配失败钩子
void vApplicationMallocFailedHook(void) {
    Serial.println("Malloc failed!");
    while (1);
}
```

## 常见设计模式

### 生产者-消费者

```c
QueueHandle_t dataQueue;

void producerTask(void *pv) {
    while (1) {
        int data = readSensor();
        xQueueSend(dataQueue, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void consumerTask(void *pv) {
    int data;
    while (1) {
        if (xQueueReceive(dataQueue, &data, portMAX_DELAY) == pdTRUE) {
            processData(data);
        }
    }
}
```

### 中断驱动 + 任务处理（ deferred interrupt ）

```c
// ISR：只做最少工作，通知任务处理
void EXTI0_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 任务：阻塞等待信号量，做耗时处理
void buttonTask(void *pv) {
    while (1) {
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) {
            // 消抖 + 处理（ISR 中不能做的耗时操作）
            vTaskDelay(pdMS_TO_TICKS(20));
            if (digitalRead(BUTTON_PIN) == LOW) {
                handleButtonPress();
            }
        }
    }
}
```

### 看门狗任务

```c
void watchdogTask(void *pv) {
    // 启用独立看门狗（IWDG），超时 1s
    IWDG->KR = 0x5555; IWDG->PR = 4; IWDG->RLR = 250; IWDG->KR = 0xCCCC;

    while (1) {
        // 检查所有关键任务的心跳标志
        if (allTasksAlive()) {
            IWDG->KR = 0xAAAA;  // 喂狗
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## 调试与排查

### 查看任务状态（调试用）

```c
// 打印所有任务状态（需 configUSE_TRACE_FACILITY 和 configUSE_STATS_FORMATTING_FUNCTIONS）
char buf[1024];
vTaskList(buf);
Serial.println(buf);
// 输出：任务名、状态（X=运行/R=就绪/B=阻塞/S=挂起/D=删除）、优先级、剩余栈、任务号

// 查看任务运行时间（需 configGENERATE_RUN_TIME_STATS）
vTaskGetRunTimeStats(buf);
Serial.println(buf);
// 输出：每个任务的 CPU 占用百分比
```

### 常见问题

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 程序卡在 HardFault | 栈溢出或 ISR 中调用非 FromISR API | 开启栈溢出检测；检查 ISR 中 API |
| 高优先级任务不运行 | 低优先级任务持锁不释放或死循环不让出 CPU | 检查互斥量释放；确保低优先级任务有阻塞调用 |
| 队列数据丢失 | 队列满且发送超时设为 0 | 增大队列；发送设合理超时 |
| 优先级反转 | 高优先级任务等低优先级持有的互斥量 | 使用互斥量（自带优先级继承）而非二值信号量 |
| 死锁 | 两个任务互相等待对方持有的锁 | 按固定顺序加锁；设置获取超时；减少锁粒度 |
| 系统不启动 | 堆太小或 `vTaskStartScheduler` 后内存不足 | 增大 `configTOTAL_HEAP_SIZE`；检查 `malloc failed hook` |
| 中断不触发 | FreeRTOS 管理的中断优先级范围配置错误 | 确保中断优先级数值 ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` |
| 定时器不回调 | 定时器服务任务栈太小或优先级太低 | 增大 `configTIMER_TASK_STACK_DEPTH`；提高 `configTIMER_TASK_PRIORITY` |

## 关键配置项（FreeRTOSConfig.h）

```c
#define configUSE_PREEMPTION            1   // 抢占式调度
#define configUSE_TIME_SLICING          1   // 同优先级时间片轮转
#define configCPU_CLOCK_HZ              168000000  // CPU 主频
#define configTICK_RATE_HZ              1000  // SysTick 频率（1ms）
#define configMAX_PRIORITIES            7   // 最大优先级数（0~6）
#define configMINIMAL_STACK_SIZE        128 // 空闲任务栈（字）
#define configTOTAL_HEAP_SIZE           32768  // 堆大小（字节）
#define configMAX_TASK_NAME_LEN         16  // 任务名最大长度
#define configUSE_MUTEXES               1   // 启用互斥量
#define configUSE_RECURSIVE_MUTEXES     1   // 启用递归互斥量
#define configUSE_COUNTING_SEMAPHORES   1   // 启用计数信号量
#define configUSE_QUEUE_SETS            1   // 启用队列集
#define configUSE_EVENT_GROUPS          1   // 启用事件组
#define configUSE_TIMERS                1   // 启用软件定时器
#define configTIMER_TASK_PRIORITY       5   // 定时器服务任务优先级
#define configTIMER_QUEUE_LENGTH        10  // 定时器命令队列长度
#define configTIMER_TASK_STACK_DEPTH    128 // 定时器任务栈
#define configCHECK_FOR_STACK_OVERFLOW  2   // 栈溢出检测
#define configUSE_MALLOC_FAILED_HOOK    1   // 内存分配失败钩子
#define configUSE_IDLE_HOOK             0   // 空闲钩子
#define configUSE_TICK_HOOK             0   // 滴答钩子
#define configUSE_TRACE_FACILITY        1   // 调试追踪
#define configUSE_STATS_FORMATTING_FUNCTIONS 1  // vTaskList 等
#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_xTimerPendFunctionCall  1
```

## 参考资源

- [FreeRTOS 官方文档](https://www.freertos.org/Documentation/16000_mastering_the_FreeRTOS_real_time_kernel.html)
- [FreeRTOS API 参考](https://www.freertos.org/a00106.html)
- [Mastering the FreeRTOS Real Time Kernel（PDF）](https://www.freertos.org/Documentation/16000_mastering_the_FreeRTOS_real_time_kernel.pdf)
