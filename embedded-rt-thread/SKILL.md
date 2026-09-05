---
name: embedded-rt-thread
description: RT-Thread 实时操作系统开发指南，覆盖内核配置、线程管理、调度器、IPC（信号量/互斥量/事件集/邮箱/消息队列）、内存管理、设备驱动框架、FinSH 控制台、组件（DFS/网络/Sensor/Audio）、ENV 工具与 SCons 构建系统。适用于 STM32/ESP32/RISC-V 等平台的 RT-Thread Nano/标准版开发。用户提及 RT-Thread、RTT、FinSH、rtthread、国产 RTOS 时使用。
---

# RT-Thread 开发指南

## 概述

RT-Thread 是一款国产开源实时操作系统，包含 Nano 版（极简内核，3KB Flash/1.2KB RAM）和标准版（完整组件生态）。本 Skill 覆盖内核 API、设备驱动框架、FinSH 调试和常用组件。

## 核心规则

- **线程函数不返回**：线程入口函数必须是死循环，退出时调用 `rt_thread_delete()`。
- **中断中调用中断安全 API**：ISR 中使用 `rt_sem_release()` 等（RT-Thread 大部分 IPC API 可在中断中调用，但 `rt_thread_mdelay()` 等阻塞 API 不行）。
- **优先级数值越小优先级越高**：RT-Thread 中 0 是最高优先级，最低优先级由 `RT_THREAD_PRIORITY_MAX - 1` 决定（通常 255）。
- **自动初始化**：使用 `INIT_APP_EXPORT()` / `INIT_DEVICE_EXPORT()` 等宏声明初始化函数，无需手动调用。
- **设备框架优先**：访问外设使用 `rt_device_find()` + `rt_device_read/write()`，不要直接操作寄存器。

## 快速开始

### 获取源码与环境

```bash
# 标准版（含组件）
git clone https://github.com/RT-Thread/rt-thread.git
cd rt-thread

# Nano 版（极简内核，可直接拷贝到工程）
git clone https://github.com/RT-Thread/rt-thread.git
# 复制 rt-thread/libcpu/arm/cortex-m4/ 和 rt-thread/src/ 到工程

# ENV 工具（Windows 下使用 menuconfig）
# 下载：https://github.com/RT-Thread/env/releases
# 解压后运行 env.exe，在 bsp 目录下输入 menuconfig
```

### 基于 STM32 BSP 创建工程

```bash
cd rt-thread/bsp/stm32/stm32f407-atk-explorer  # 或对应板卡

# 配置
scons --menuconfig
# RT-Thread Kernel → 内核配置
# RT-Thread Components → 组件配置
# Hardware Drivers Config → 硬件驱动配置

# 编译（Keil 工程）
scons --target=mdk5      # 生成 Keil 工程
# 或直接编译
scons -j8

# 生成 VSCode / Eclipse 工程
scons --target=vsc
scons --target=eclipse
```

### 第一个线程

```c
#include <rtthread.h>

#define THREAD_PRIORITY    25
#define THREAD_STACK_SIZE  512
#define THREAD_TIMESLICE   5

static rt_thread_t tid1 = RT_NULL;

static void thread1_entry(void *parameter) {
    rt_uint32_t count = 0;
    while (1) {
        rt_kprintf("thread1 count: %d\n", count++);
        rt_thread_mdelay(500);  // 毫秒级延时，让出 CPU
    }
}

int main(void) {
    tid1 = rt_thread_create("thread1",    // 线程名
                            thread1_entry, // 入口函数
                            RT_NULL,       // 参数
                            THREAD_STACK_SIZE,  // 栈大小
                            THREAD_PRIORITY,    // 优先级
                            THREAD_TIMESLICE);  // 时间片

    if (tid1 != RT_NULL) {
        rt_thread_startup(tid1);  // 启动线程
    }
    return 0;
}
```

## 线程管理

### 创建与删除

```c
// 动态创建（从堆分配）
rt_thread_t rt_thread_create(const char *name,
                              void (*entry)(void *parameter),
                              void *parameter,
                              rt_uint32_t stack_size,
                              rt_uint8_t priority,
                              rt_uint32_t tick);

// 静态创建（用户提供内存）
rt_err_t rt_thread_init(struct rt_thread *thread,
                         const char *name,
                         void (*entry)(void *parameter),
                         void *parameter,
                         void *stack_start,
                         rt_uint32_t stack_size,
                         rt_uint8_t priority,
                         rt_uint32_t tick);

// 启动
rt_err_t rt_thread_startup(rt_thread_t thread);

// 删除（动态创建的）
rt_err_t rt_thread_delete(rt_thread_t thread);

// 脱离（静态创建的）
rt_err_t rt_thread_detach(rt_thread_t thread);
```

### 线程控制

```c
// 当前线程延时（单位：系统节拍，通常 1ms = 1 tick）
rt_err_t rt_thread_delay(rt_tick_t tick);
rt_err_t rt_thread_mdelay(rt_int32_t ms);  // 毫秒延时
rt_err_t rt_thread_udelay(rt_int32_t us);  // 微秒延时（忙等）

// 让出 CPU（同优先级线程调度）
rt_err_t rt_thread_yield(void);

// 挂起/恢复
rt_err_t rt_thread_suspend(rt_thread_t thread);
rt_err_t rt_thread_resume(rt_thread_t thread);

// 控制（优先级/栈等）
rt_err_t rt_thread_control(rt_thread_t thread, int cmd, void *arg);
// RT_THREAD_CTRL_CHANGE_PRIORITY：改优先级
// RT_THREAD_CTRL_STARTUP：启动
// RT_THREAD_CTRL_CLOSE：关闭

// 获取当前线程
rt_thread_t rt_thread_self(void);

// 获取线程信息
rt_uint8_t rt_thread_get_priority(rt_thread_t thread);
```

### 线程状态

```
初始 → 创建后未启动
就绪 → 等待 CPU 运行
运行 → 正在运行
挂起 → 阻塞（等待信号量/延时等）
关闭 → 已结束，等待回收
```

## 调度器

```c
// 启动调度器（main 函数返回后自动启动，通常不需手动调用）
void rt_system_scheduler_start(void);

// 调度器配置（rtconfig.h）
#define RT_THREAD_PRIORITY_MAX  32     // 最大优先级数（8/32/256）
#define RT_TICK_PER_SECOND      1000   // 每秒节拍数（1ms 一拍）
#define RT_USING_PREEMPT              // 抢占式调度
#define RT_USING_TIME_SLICE           // 时间片轮转
#define RT_USING_OVERFLOW_CHECK       // 栈溢出检测
```

## IPC（进程间通信）

### 信号量（Semaphore）

```c
// 创建动态信号量
rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag);
// flag: RT_IPC_FLAG_FIFO（先进先出）/ RT_IPC_FLAG_PRIO（按优先级）

// 删除
rt_err_t rt_sem_delete(rt_sem_t sem);

// 获取（P 操作，可阻塞）
rt_err_t rt_sem_take(rt_sem_t sem, rt_int32_t time);
// time = 0：不等待；RT_WAITING_FOREVER：永久等待

// 释放（V 操作）
rt_err_t rt_sem_release(rt_sem_t sem);

// 示例：中断同步
static rt_sem_t sem;
void EXTI_IRQHandler(void) {
    rt_sem_release(sem);  // 中断中释放
}
void task_entry(void *p) {
    while (1) {
        rt_sem_take(sem, RT_WAITING_FOREVER);  // 等待中断
        handle_event();
    }
}
```

### 互斥量（Mutex，支持优先级继承）

```c
rt_mutex_t rt_mutex_create(const char *name, rt_uint8_t flag);
rt_err_t rt_mutex_delete(rt_mutex_t mutex);

rt_err_t rt_mutex_take(rt_mutex_t mutex, rt_int32_t time);
rt_err_t rt_mutex_release(rt_mutex_t mutex);

// 示例：保护共享资源
static rt_mutex_t spi_mutex;
void spi_transfer(int dev, uint8_t *tx, uint8_t *rx, int len) {
    rt_mutex_take(spi_mutex, RT_WAITING_FOREVER);
    cs_low(dev);
    spi_rw(tx, rx, len);
    cs_high(dev);
    rt_mutex_release(spi_mutex);
}
```

### 事件集（Event Group）

```c
rt_event_t rt_event_create(const char *name, rt_uint8_t flag);
rt_err_t rt_event_delete(rt_event_t event);

// 发送事件
rt_err_t rt_event_send(rt_event_t event, rt_uint32_t set);

// 接收事件
rt_err_t rt_event_recv(rt_event_t event,
                        rt_uint32_t set,
                        rt_uint8_t option,
                        rt_int32_t timeout,
                        rt_uint32_t *recved);
// option: RT_EVENT_FLAG_OR（任意位）/ RT_EVENT_FLAG_AND（全部位）
//         | RT_EVENT_FLAG_CLEAR（接收后清除）

#define EVENT_SENSOR_READY  (1 << 0)
#define EVENT_NET_READY     (1 << 1)

// 任务中等待两个事件都就绪
rt_uint32_t recved;
rt_event_recv(event, EVENT_SENSOR_READY | EVENT_NET_READY,
              RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
              RT_WAITING_FOREVER, &recved);
```

### 邮箱（Mailbox，固定 4 字节/封）

```c
rt_mailbox_t rt_mb_create(const char *name, rt_size_t size, rt_uint8_t flag);
rt_err_t rt_mb_delete(rt_mailbox_t mb);

rt_err_t rt_mb_send(rt_mailbox_t mb, rt_uint32_t value);
rt_err_t rt_mb_send_wait(rt_mailbox_t mb, rt_uint32_t value, rt_int32_t timeout);
rt_err_t rt_mb_recv(rt_mailbox_t mb, rt_uint32_t *value, rt_int32_t timeout);

// 适合传递指针或小整数
struct msg { int type; void *data; };
struct msg *m = rt_malloc(sizeof(struct msg));
rt_mb_send(mb, (rt_uint32_t)m);  // 发送指针
```

### 消息队列（Message Queue，可变长度）

```c
rt_mq_t rt_mq_create(const char *name, rt_size_t msg_size,
                      rt_size_t max_msgs, rt_uint8_t flag);
rt_err_t rt_mq_delete(rt_mq_t mq);

rt_err_t rt_mq_send(rt_mq_t mq, void *buffer, rt_size_t size);
rt_err_t rt_mq_send_wait(rt_mq_t mq, void *buffer, rt_size_t size, rt_int32_t timeout);
rt_err_t rt_mq_recv(rt_mq_t mq, void *buffer, rt_size_t size, rt_int32_t timeout);

// 示例
struct sensor_data { float temp; float humi; };
struct sensor_data data = {25.5, 60.0};
rt_mq_send(mq, &data, sizeof(data));

struct sensor_data recv;
rt_mq_recv(mq, &recv, sizeof(recv), RT_WAITING_FOREVER);
```

## 内存管理

### 堆内存（动态分配）

```c
void *rt_malloc(rt_size_t size);
void rt_free(void *ptr);
void *rt_realloc(void *ptr, rt_size_t size);
void *rt_calloc(rt_size_t count, rt_size_t size);

// 内存信息
rt_size_t rt_memory_peripherals(void);  // 已使用
void rt_memory_info(rt_size_t *total, rt_size_t *used, rt_size_t *max_used);

// 配置（rtconfig.h）
#define RT_USING_HEAP
#define RT_USING_SMALL_MEM        // 小内存管理算法
// #define RT_USING_SLAB            // SLAB 算法（多内存池）
// #define RT_USING_MEMHEAP         // 内存堆（支持多片内存）
```

### 内存池（固定大小，无碎片）

```c
rt_mempool_t rt_mp_create(const char *name, rt_size_t block_count, rt_size_t block_size);
rt_err_t rt_mp_delete(rt_mempool_t mp);

void *rt_mp_alloc(rt_mempool_t mp, rt_int32_t time);
void rt_mp_free(void *block);
```

## 设备驱动框架

### 设备模型

```c
// 查找设备
rt_device_t rt_device_find(const char *name);

// 打开/关闭
rt_err_t rt_device_open(rt_device_t dev, rt_uint16_t oflag);
rt_err_t rt_device_close(rt_device_t dev);
// oflag: RT_DEVICE_OFLAG_RDONLY / WRONLY / RDWR
//        RT_DEVICE_FLAG_STREAM / RT_DEVICE_FLAG_INT_RX / RT_DEVICE_FLAG_DMA_RX

// 读写
rt_size_t rt_device_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size);
rt_size_t rt_device_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size);

// 控制
rt_err_t rt_device_control(rt_device_t dev, int cmd, void *arg);

// 设置接收回调
rt_err_t rt_device_set_rx_indicate(rt_device_t dev,
                                     rt_err_t (*rx_ind)(rt_device_t dev, rt_size_t size));
```

### UART 示例

```c
#include <rtthread.h>
#include <rtdevice.h>

#define SAMPLE_UART_NAME "uart1"
static struct rt_semaphore rx_sem;
static rt_device_t serial;

// 接收回调（中断上下文）
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size) {
    rt_sem_release(&rx_sem);  // 通知任务有数据
    return RT_EOK;
}

// 串口接收任务
static void uart_thread_entry(void *parameter) {
    char ch;
    while (1) {
        rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
        while (rt_device_read(serial, -1, &ch, 1) == 1) {
            rt_device_write(serial, 0, &ch, 1);  // 回显
        }
    }
}

int uart_sample(void) {
    rt_sem_init(&rx_sem, "rx_sem", 0, RT_IPC_FLAG_FIFO);

    serial = rt_device_find(SAMPLE_UART_NAME);
    if (!serial) {
        rt_kprintf("uart %s not found!\n", SAMPLE_UART_NAME);
        return -RT_ERROR;
    }

    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);  // 中断接收模式
    rt_device_set_rx_indicate(serial, uart_rx_ind);

    rt_thread_t tid = rt_thread_create("uart", uart_thread_entry,
                                        RT_NULL, 1024, 20, 10);
    rt_thread_startup(tid);
    return RT_EOK;
}
MSH_CMD_EXPORT(uart_sample, uart echo sample);  // 导出为 FinSH 命令
```

### I2C / SPI / PIN 设备

```c
// PIN 设备（GPIO）
rt_pin_mode(pin, PIN_MODE_OUTPUT);   // PIN_MODE_INPUT / INPUT_PULLUP / OUTPUT
rt_pin_write(pin, PIN_HIGH);          // PIN_LOW / PIN_HIGH
int val = rt_pin_read(pin);
rt_pin_attach_irq(pin, PIN_IRQ_MODE_RISING, callback, args);
rt_pin_irq_enable(pin, PIN_IRQ_ENABLE);

// I2C 设备
struct rt_i2c_bus_device *i2c = rt_i2c_bus_device_find("i2c1");
rt_i2c_master_send(i2c, addr, flags, buf, len);
rt_i2c_master_recv(i2c, addr, flags, buf, len);

// SPI 设备
struct rt_spi_device *spi = (struct rt_spi_device *)rt_device_find("spi10");
rt_spi_send(spi, buf, len);
rt_spi_recv(spi, buf, len);
rt_spi_send_then_recv(spi, tx, tx_len, rx, rx_len);
```

## FinSH 控制台

FinSH 是 RT-Thread 的命令行 shell，支持 C 语言表达式和 msh（传统 shell 风格）。

### 导出命令

```c
// msh 命令（推荐）
void hello_cmd(void) {
    rt_kprintf("Hello RT-Thread!\n");
}
MSH_CMD_EXPORT(hello_cmd, say hello);

// 带参数的 msh 命令
int echo_cmd(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        rt_kprintf("%s ", argv[i]);
    }
    rt_kprintf("\n");
    return 0;
}
MSH_CMD_EXPORT(echo_cmd, echo arguments);

// C 表达式风格（FinSH 传统模式）
long my_func(int a, int b) { return a + b; }
FINSH_FUNCTION_EXPORT(my_func, add two numbers);
```

### 内置命令

```bash
help              # 显示所有命令
list_thread       # 列出所有线程（状态、优先级、栈剩余）
list_sem          # 列出信号量
list_mutex        # 列出互斥量
list_event        # 列出事件集
list_mb           # 列出邮箱
list_mq           # 列出消息队列
list_timer        # 列出定时器
list_device       # 列出所有设备
list_mem          # 内存使用情况
list_msgqueue     # 消息队列
free              # 内存信息
ps                # 线程状态（同 list_thread）
top               # 线程 CPU 占用
version           # 版本信息
reboot            # 重启系统
```

### 自定义 FinSH 命令示例

```c
// 线程栈溢出检测命令
int cmd_stack_check(int argc, char **argv) {
    extern rt_list_t rt_thread_defunct;
    rt_kprintf("%-12s %-8s %-8s %-8s\n", "thread", "pri", "stack", "left");
    rt_kprintf("----------------------------------------\n");
    // 遍历线程列表...
    return 0;
}
MSH_CMD_EXPORT(cmd_stack_check, check thread stack usage);
```

## 定时器

```c
// 动态创建
rt_timer_t rt_timer_create(const char *name,
                            void (*timeout)(void *parameter),
                            void *parameter,
                            rt_tick_t time,
                            rt_uint8_t flag);
// flag: RT_TIMER_FLAG_ONE_SHOT（一次性）/ RT_TIMER_FLAG_PERIODIC（周期）
//       RT_TIMER_FLAG_SOFT_TIMER（软件定时器，在定时器线程中执行）

rt_err_t rt_timer_delete(rt_timer_t timer);
rt_err_t rt_timer_start(rt_timer_t timer);
rt_err_t rt_timer_stop(rt_timer_t timer);
rt_err_t rt_timer_control(rt_timer_t timer, int cmd, void *arg);

// 示例：周期定时器
static void timer_cb(void *param) {
    rt_kprintf("timer tick\n");
}
rt_timer_t timer = rt_timer_create("timer", timer_cb, RT_NULL,
                                    RT_TICK_PER_SECOND,  // 1秒
                                    RT_TIMER_FLAG_PERIODIC);
rt_timer_start(timer);
```

## 自动初始化机制

```c
// 按层级自动调用，无需在 main 中手动调用
INIT_BOARD_EXPORT(board_init);      // 板级初始化（最早，pure段）
INIT_PREV_EXPORT(prev_init);        // 核心组件前
INIT_DEVICE_EXPORT(device_init);    // 设备驱动
INIT_COMPONENT_EXPORT(comp_init);   // 组件
INIT_ENV_EXPORT(env_init);          // 环境
INIT_APP_EXPORT(app_init);           // 应用（最晚）

// 示例：自动初始化 LED
int led_init(void) {
    rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
    return RT_EOK;
}
INIT_DEVICE_EXPORT(led_init);
```

## DFS 文件系统

```c
#include <rtthread.h>
#include <dfs_posix.h>  // POSIX 接口

// 挂载文件系统
dfs_mount("sd0", "/", "elm", 0, 0);  // FAT 文件系统
dfs_mount("flash0", "/data", "lfs", 0, 0);  // LittleFS

// POSIX 文件操作
int fd = open("/data/test.txt", O_WRONLY | O_CREAT, 0644);
write(fd, "hello", 5);
close(fd);

fd = open("/data/test.txt", O_RDONLY);
char buf[32];
int len = read(fd, buf, sizeof(buf));
close(fd);

// 目录操作
DIR *dir = opendir("/");
struct dirent *entry;
while ((entry = readdir(dir)) != NULL) {
    rt_kprintf("%s\n", entry->d_name);
}
closedir(dir);
```

## 网络框架（Sal + LwIP）

```c
#include <rtthread.h>
#include <sys/socket.h>
#include <netdb.h>

// TCP 客户端
int tcp_client_test(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = inet_addr("192.168.1.100");
    connect(sock, (struct sockaddr*)&server, sizeof(server));

    send(sock, "hello", 5, 0);
    char buf[128];
    int len = recv(sock, buf, sizeof(buf), 0);
    closesocket(sock);
    return 0;
}
MSH_CMD_EXPORT(tcp_client_test, tcp client test);
```

## 常见问题排查

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 线程不运行 | 优先级太低被抢占、未调用 startup | 检查优先级；确认 `rt_thread_startup()` 已调用 |
| 栈溢出 | 栈太小、局部数组过大 | 增大 `stack_size`；用 `list_thread` 查看剩余栈 |
| FinSH 无输出 | 串口未初始化、控制台设备名不对 | 检查 `RT_CONSOLE_DEVICE_NAME`；确认串口驱动已注册 |
| 内存分配失败 | 堆太小、内存泄漏 | 增大 `RT_HEAP_SIZE`；用 `free` 命令查看内存 |
| 设备找不到 | 驱动未注册、设备名拼写错误 | `list_device` 查看已注册设备；检查驱动初始化 |
| 中断中崩溃 | ISR 中调用了阻塞 API | ISR 中只调用非阻塞 API（如 `rt_sem_release`） |
| 优先级反转 | 用信号量而非互斥量保护资源 | 改用 `rt_mutex_t`（支持优先级继承） |
| 定时器不触发 | 未 start、时间单位错误 | 确认 `rt_timer_start()`；时间单位是 tick 不是 ms |

## 参考资源

- [RT-Thread 官方文档](https://www.rt-thread.org/document/site/)
- [RT-Thread GitHub](https://github.com/RT-Thread/rt-thread)
- [RT-Thread 编程指南](https://www.rt-thread.org/document/site/programming-manual/introduction/introduction/)
- [ENV 工具下载](https://github.com/RT-Thread/env/releases)
