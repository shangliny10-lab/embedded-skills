---
name: embedded-zephyr
description: Zephyr RTOS 开发指南，覆盖 Zephyr 环境搭建（West）、工程结构、设备树（DTS）、Kconfig 配置、线程管理、内核服务（信号量/互斥量/消息队列/事件）、设备驱动模型、Devicetree 绑定、日志系统（Logging）、Shell、网络栈（LwIP）、蓝牙 BLE。适用于 nRF52/nRF91/STM32/ESP32 等平台的 Zephyr 应用开发。用户提及 Zephyr、west、nRF Connect、Nordic、Zephyr RTOS 时使用。
---

# Zephyr RTOS 开发指南

## 概述

Zephyr 是 Linux 基金会旗下的开源实时操作系统，采用组件化设计，支持多种架构（ARM/RISC-V/Xtensa/MIPS 等），内置蓝牙、网络、文件系统等子系统。Nordic nRF 系列 SDK 基于 Zephyr。

## 核心规则

- **使用 West 管理**：所有 Zephyr 工程通过 `west` 工具管理，不要手动 git clone 内核源码。
- **配置通过 Kconfig + Devicetree**：功能开关用 `prj.conf`（Kconfig），硬件描述用 `app.overlay`（设备树覆盖），不要改内核源码。
- **设备树获取配置**：驱动和应用通过 `DT_PROP()`、`DEVICE_DT_GET()` 等宏从设备树获取硬件信息，不要硬编码引脚。
- **日志用 LOG_MODULE_REGISTER**：不要用 `printf`，使用 Zephyr Logging 子系统，支持级别控制和后端切换。
- **线程栈用 K_THREAD_STACK_DEFINE**：栈内存必须用专用宏定义，确保对齐和 MPU 保护。

## 快速开始

### 环境搭建

```bash
# 安装 West（Python pip）
pip install west

# 初始化工作区
west init myzephyrworkspace
cd myzephyrworkspace
west update  # 下载 Zephyr 源码和依赖模块

# 安装 Zephyr SDK（工具链）
# 下载：https://github.com/zephyrproject-rtos/sdk-ng/releases
# Linux:
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.4/zephyr-sdk-0.16.4_linux-x86_64.tar.xz
tar xvf zephyr-sdk-0.16.4_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.4
./setup.sh

# Windows: 使用 Zephyr SDK 安装包或 nRF Connect for Desktop
```

### 创建应用

```bash
# 方式一：从示例复制
cp -r zephyr/samples/basic/blinky my_app
cd my_app

# 方式二：手动创建最小工程
# my_app/
# ├── src/
# │   └── main.c
# ├── CMakeLists.txt
# ├── prj.conf
# └── app.overlay (可选)
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_app)

target_sources(app PRIVATE src/main.c)
# 添加源文件目录
# add_subdirectory(src/drivers)
```

### prj.conf（Kconfig 配置）

```conf
# 内核
CONFIG_MAIN_THREAD_PRIORITY=7
CONFIG_MAIN_STACK_SIZE=1024
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=2048

# 日志
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_DEFAULT_LEVEL=3  # 0=none 1=err 2=wrn 3=inf 4=dbg

# 串口控制台
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_SERIAL=y

# GPIO
CONFIG_GPIO=y

# 可选：网络
# CONFIG_NETWORKING=y
# CONFIG_NET_IPV4=y
# CONFIG_NET_DHCPV4=y

# 可选：蓝牙
# CONFIG_BT=y
# CONFIG_BT_PERIPHERAL=y
```

### 编译与烧录

```bash
# 编译（指定板卡）
west build -b stm32f4_disco .
# 或 nRF52840: west build -b nrf52840dk_nrf52840 .
# 或 ESP32: west build -b esp32_devkitc_wroom .

# 清理
west build -t clean
# 或 rm -rf build

# 烧录
west flash
# 指定调试器：west flash --runner jlink
# 仅查看命令：west flash --context

# 串口监视
west espressif monitor  # ESP32
# 或使用 minicom/picocom: picocom -b 115200 /dev/ttyUSB0
```

### 最小应用（Blinky）

```c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

// 从设备树获取 LED0 节点（通常在板级 dts 中定义）
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void) {
    LOG_INF("Blinky application started");

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED device not ready");
        return 0;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(500);
    }
    return 0;
}
```

## 线程管理

### 创建线程

```c
#include <zephyr/kernel.h>

// 定义栈（必须用 K_THREAD_STACK_DEFINE）
#define MY_THREAD_STACK_SIZE 1024
K_THREAD_STACK_DEFINE(my_thread_stack, MY_THREAD_STACK_SIZE);
static struct k_thread my_thread_data;

// 线程入口函数
void my_thread_entry(void *p1, void *p2, void *p3) {
    while (1) {
        printk("My thread running\n");
        k_msleep(1000);
    }
}

// 启动线程
k_tid_t my_tid = k_thread_create(
    &my_thread_data,           // 线程数据结构
    my_thread_stack,            // 栈
    K_THREAD_STACK_SIZEOF(my_thread_stack),  // 栈大小
    my_thread_entry,            // 入口函数
    NULL, NULL, NULL,           // 三个参数
    5,                          // 优先级（数值越小越高，-1~-16为协作式）
    0,                          // 选项（K_ESSENTIAL/K_USER/K_INHERIT_PERMS）
    K_NO_WAIT                   // 启动延迟（K_NO_WAIT=立即，K_MSEC(100)=100ms后）
);

// 线程控制
k_thread_start(my_tid);           // 启动（如果创建时延迟了）
k_thread_suspend(my_tid);         // 挂起
k_thread_resume(my_tid);          // 恢复
k_thread_abort(my_tid);           // 终止
k_sleep(K_MSEC(100));             // 当前线程睡眠
k_yield();                         // 让出 CPU
k_msleep(100);                     // 毫秒睡眠
k_usleep(100);                     // 微秒睡眠（忙等）
```

### 线程选项

```c
K_ESSENTIAL          // 关键线程，终止会导致系统 panic
K_USER               // 用户模式线程（需 MPU 支持）
K_INHERIT_PERMS      // 继承父线程权限
K_FP_REG             // 使用浮点寄存器
```

### 系统工作队列（延迟执行）

```c
// 定义工作项
static struct k_work my_work;

void work_handler(struct k_work *work) {
    // 在系统工作队列线程中执行
    printk("Work executed\n");
}

// 初始化并提交
k_work_init(&my_work, work_handler);
k_work_submit(&my_work);  // 提交到系统工作队列

// 延迟工作
static struct k_work_delayable my_delayed_work;
k_work_init_delayable(&my_delayed_work, work_handler);
k_work_schedule(&my_delayed_work, K_SECONDS(5));  // 5秒后执行
k_work_cancel_delayable(&my_delayed_work);          // 取消
```

## 内核服务

### 信号量

```c
K_SEM_DEFINE(my_sem, 0, 1);  // 初始值0，最大值1

k_sem_give(&my_sem);          // V 操作（释放）
k_sem_take(&my_sem, K_FOREVER);  // P 操作（获取，永久等待）
k_sem_take(&my_sem, K_MSEC(100));  // 超时100ms
int count = k_sem_count_get(&my_sem);  // 当前值
k_sem_reset(&my_sem);         // 重置为0
```

### 互斥量

```c
K_MUTEX_DEFINE(my_mutex);

k_mutex_lock(&my_mutex, K_FOREVER);
// 访问共享资源
k_mutex_unlock(&my_mutex);
```

### 消息队列（Message Queue）

```c
struct my_msg {
    int id;
    float value;
};

K_MSGQ_DEFINE(my_msgq, sizeof(struct my_msg), 10, 4);  // 元素大小，最大数量，对齐

struct my_msg msg = {1, 3.14f};
k_msgq_put(&my_msgq, &msg, K_FOREVER);  // 发送

struct my_msg recv;
k_msgq_get(&my_msgq, &recv, K_FOREVER);  // 接收

int pending = k_msgq_num_used_get(&my_msgq);  // 待处理消息数
```

### 事件（Events）

```c
K_EVENT_DEFINE(my_event);

#define EVENT_SENSOR_READY  BIT(0)
#define EVENT_NET_READY     BIT(1)

k_event_post(&my_event, EVENT_SENSOR_READY);  // 触发事件

uint32_t events = k_event_wait(
    &my_event,
    EVENT_SENSOR_READY | EVENT_NET_READY,  // 等待的位
    false,                                   // false=任意位满足，true=全部满足
    K_FOREVER                                // 超时
);
```

### 邮箱（Mailbox，可传递大数据）

```c
K_MBOX_DEFINE(my_mbox);

// 发送
struct k_mbox_msg tx_msg = {
    .size = sizeof(data),
    .tx_data = &data,
};
k_mbox_put(&my_mbox, &tx_msg, K_FOREVER);

// 接收
struct k_mbox_msg rx_msg;
k_mbox_get(&my_mbox, &rx_msg, &buffer, K_FOREVER);
```

### 定时器

```c
static struct k_timer my_timer;

void timer_expiry(struct k_timer *timer) {
    // 超时回调（在中断上下文！不要阻塞）
    printk("Timer expired\n");
}

void timer_stop(struct k_timer *timer) {
    // 停止回调（可选）
}

k_timer_init(&my_timer, timer_expiry, timer_stop);
k_timer_start(&my_timer, K_SECONDS(1), K_SECONDS(1));  // 1秒后开始，周期1秒
// 第二个参数为0表示一次性定时器
k_timer_stop(&my_timer);
int remaining = k_timer_remaining_get(&my_timer);  // 剩余时间（ms）
```

## 设备树（Devicetree）

### 应用覆盖文件 app.overlay

```dts
/* 覆盖或追加板级设备树 */

/ {
    /* 自定义 LED 别名 */
    aliases {
        my-led = &myled;
    };

    /* 自定义节点 */
    myled: myled {
        compatible = "gpio-leds";
        led0 {
            gpios = <&gpiob 5 GPIO_ACTIVE_HIGH>;
            label = "My LED";
        };
    };

    /* 自定义按键 */
    mykeys {
        compatible = "gpio-keys";
        button0: button_0 {
            gpios = <&gpioc 13 (GPIO_PULL_UP | GPIO_ACTIVE_LOW)>;
            label = "User Button";
        };
    };
};

/* 启用外设 */
&usart2 {
    status = "okay";
    current-speed = <115200>;
};

&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;  // 400kHz

    /* 挂载 I2C 设备 */
    bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
    };
};

&spi2 {
    status = "okay";
    cs-gpios = <&gpiob 12 GPIO_ACTIVE_LOW>;
    w25q32: w25q32@0 {
        compatible = "jedec,spi-nor";
        reg = <0>;
        spi-max-frequency = <40000000>;
        size = <33554432>;  // 32MB
    };
};
```

### 代码中访问设备树

```c
#include <zephyr/devicetree.h>

// 获取别名节点
#define LED_NODE DT_ALIAS(led0)

// 获取属性
#define LED_GPIO_PORT   DT_GPIO_LABEL(LED_NODE, gpios)
#define LED_GPIO_PIN    DT_GPIO_PIN(LED_NODE, gpios)
#define LED_GPIO_FLAGS  DT_GPIO_FLAGS(LED_NODE, gpios)

// 更简洁的方式：获取设备指针
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

// 检查节点是否存在
#if DT_NODE_HAS_STATUS(LED_NODE, okay)
// 节点启用
#else
#error "LED node not found or disabled"
#endif

// 遍历子节点
#define CHILD_NODE(i) DT_CHILD(DT_PATH(soc, i2c@40005400), i)
// 或使用 DT_FOREACH_CHILD
```

## 设备驱动模型

### 获取设备并使用

```c
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>

// GPIO
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
gpio_pin_set_dt(&led, 1);
gpio_pin_toggle_dt(&led);

// UART
const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(usart2));
uart_poll_out(uart, 'A');  // 轮询发送
// 中断/DMA 方式需配置回调

// I2C
const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c1));
uint8_t reg = 0x75;
uint8_t data;
i2c_write_read(i2c, 0x68, &reg, 1, &data, 1);

// SPI
const struct device *spi = DEVICE_DT_GET(DT_NODELABEL(spi2));
struct spi_config cfg = {
    .frequency = 1000000,
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL | SPI_MODE_CPHA,
    .cs = NULL,  // 或 GPIO 片选
};
uint8_t tx[2] = {0x01, 0x02};
uint8_t rx[2];
struct spi_buf tx_buf = {.buf = tx, .len = 2};
struct spi_buf rx_buf = {.buf = rx, .len = 2};
struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};
spi_transceive(spi, &cfg, &tx_set, &rx_set);
```

### 自定义驱动（简化）

```c
// drivers/sensor/mysensor/mysensor.c
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mysensor, LOG_LEVEL_INF);

struct mysensor_config {
    struct gpio_dt_spec data_pin;
};

struct mysensor_data {
    int value;
};

static int mysensor_init(const struct device *dev) {
    const struct mysensor_config *cfg = dev->config;
    if (!gpio_is_ready_dt(&cfg->data_pin)) {
        LOG_ERR("GPIO not ready");
        return -ENODEV;
    }
    gpio_pin_configure_dt(&cfg->data_pin, GPIO_INPUT);
    return 0;
}

static int mysensor_read(const struct device *dev, int *value) {
    const struct mysensor_config *cfg = dev->config;
    struct mysensor_data *data = dev->data;
    data->value = gpio_pin_get_dt(&cfg->data_pin);
    *value = data->value;
    return 0;
}

// 设备树绑定宏
#define MYSCENSOR_INIT(i)                                              \
    static const struct mysensor_config mysensor_cfg_##i = {          \
        .data_pin = GPIO_DT_SPEC_INST_GET(i, data_gpios),            \
    };                                                                  \
    static struct mysensor_data mysensor_data_##i;                     \
    DEVICE_DT_INST_DEFINE(i, mysensor_init, NULL,                     \
                          &mysensor_data_##i, &mysensor_cfg_##i,     \
                          POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MYSCENSOR_INIT)
```

## 日志系统

```c
#include <zephyr/logging/log.h>

// 注册模块（在 .c 文件顶部）
LOG_MODULE_REGISTER(my_module, LOG_LEVEL_INF);

// 使用
LOG_ERR("Error: %d", code);     // 错误
LOG_WRN("Warning: %s", msg);     // 警告
LOG_INF("Info: value=%d", val);  // 信息
LOG_DBG("Debug: %p", ptr);       // 调试（默认不输出）

// 条件日志
LOG_HEXDUMP_INF(data, len, "Data:");  // 十六进制转储

// 运行时控制
#include <zephyr/logging/log_ctrl.h>
log_filter_set(NULL, LOG_LEVEL_DBG);  // 设置全局级别
```

### prj.conf 日志配置

```conf
CONFIG_LOG=y
CONFIG_LOG_MODE_DEFERRED=y      # 延迟模式（性能好，需调用 log_process()）
# CONFIG_LOG_MODE_IMMEDIATE=y   # 立即模式（调试用，可能影响实时性）
CONFIG_LOG_BACKEND_UART=y        # UART 后端
CONFIG_LOG_BACKEND_RTT=y         # RTT 后端（SEGGER）
CONFIG_LOG_DEFAULT_LEVEL=3       # 默认级别
CONFIG_LOG_MAX_LEVEL=4           # 编译最大级别
```

## Shell

```c
#include <zephyr/shell/shell.h>

// 自定义命令
static int cmd_hello(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "Hello from shell! args=%d", argc);
    return 0;
}

// 带参数的命令
static int cmd_echo(const struct shell *sh, size_t argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        shell_print(sh, "arg[%d]: %s", i, argv[i]);
    }
    return 0;
}

// 注册命令
SHELL_CMD_REGISTER(hello, NULL, "Say hello", cmd_hello);
SHELL_CMD_ARG_REGISTER(echo, NULL, "Echo arguments", cmd_echo, 1, 3);  // 最少1个，最多3个参数

// 子命令
static int cmd_led_on(const struct shell *sh, size_t argc, char **argv) {
    gpio_pin_set_dt(&led, 1);
    return 0;
}
static int cmd_led_off(const struct shell *sh, size_t argc, char **argv) {
    gpio_pin_set_dt(&led, 0);
    return 0;
}
SHELL_STATIC_SUBCMD_SET_CREATE(led_cmds,
    SHELL_CMD(on, NULL, "Turn LED on", cmd_led_on),
    SHELL_CMD(off, NULL, "Turn LED off", cmd_led_off),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &led_cmds, "LED control", NULL);
```

### 启用 Shell

```conf
# prj.conf
CONFIG_SHELL=y
CONFIG_SHELL_BACKEND_SERIAL=y
CONFIG_SHELL_BACKEND_SERIAL_INTERRUPT_DRIVEN=y
CONFIG_SHELL_THREAD_PRIORITY=14
CONFIG_SHELL_STACK_SIZE=2048
```

## 网络（LwIP）

```c
#include <zephyr/net/socket.h>

// TCP 客户端
int tcp_client(void) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
    };
    net_addr_pton(AF_INET, "192.168.1.100", &server.sin_addr);
    connect(sock, (struct sockaddr*)&server, sizeof(server));

    send(sock, "hello", 5, 0);
    char buf[128];
    int len = recv(sock, buf, sizeof(buf), 0);
    close(sock);
    return 0;
}

// HTTP 请求（使用 http_client 库更方便）
```

### 网络配置

```conf
CONFIG_NETWORKING=y
CONFIG_NET_IPV4=y
CONFIG_NET_DHCPV4=y
CONFIG_NET_TCP=y
CONFIG_NET_UDP=y
CONFIG_NET_SOCKETS=y
CONFIG_NET_SOCKETS_POSIX_NAMES=y
CONFIG_DNS_RESOLVER=y
CONFIG_HTTP_CLIENT=y
CONFIG_MQTT_LIB=y
```

## 蓝牙 BLE（Peripheral 示例）

```c
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define DEVICE_NAME "Zephyr BLE"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) printk("Connection failed (err %u)\n", err);
    else printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason %u)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_init(void) {
    int err = bt_enable(NULL);
    if (err) { printk("Bluetooth init failed (err %d)\n", err); return err; }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) { printk("Advertising failed to start (err %d)\n", err); return err; }

    printk("Bluetooth initialized, advertising\n");
    return 0;
}
```

## 常见问题排查

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 编译找不到 Zephyr | 未设置 ZEPHYR_BASE 或 west 环境 | 在 west 工作区内执行；或 `source zephyr-env.sh` |
| 设备树节点找不到 | 别名未定义、节点 status 不是 okay | 检查 `app.overlay`；用 `DT_NODE_HAS_STATUS` 宏检查 |
| 日志无输出 | 日志后端未启用、级别太低 | `prj.conf` 启用 `CONFIG_LOG_BACKEND_UART`；提高 `CONFIG_LOG_DEFAULT_LEVEL` |
| 线程栈溢出 | 栈太小、局部变量过大 | 增大 `K_THREAD_STACK_DEFINE` 大小；启用 `CONFIG_STACK_SENTINEL` |
| 硬件不工作 | 设备树配置错误、引脚冲突 | 检查 `app.overlay` 引脚；查看编译生成的 `build/zephyr/zephyr.dts` |
| 烧录失败 | 调试器驱动、板卡选择错误 | 确认 `west build -b <board>` 正确；检查 J-Link/ST-Link 驱动 |
| 网络不通 | DHCP 未获取、MAC 冲突 | 检查 `CONFIG_NET_DHCPV4`；用 `net iface` 命令查看 |
| 蓝牙不广播 | BT 未初始化、协议栈配置不全 | 确认 `bt_enable()` 返回 0；检查 `CONFIG_BT=y` |

## 参考资源

- [Zephyr 官方文档](https://docs.zephyrproject.org/)
- [Zephyr GitHub](https://github.com/zephyrproject-rtos/zephyr)
- [nRF Connect SDK](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/)
- [Zephyr 示例](https://github.com/zephyrproject-rtos/zephyr/tree/main/samples)
