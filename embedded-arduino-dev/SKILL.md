---
name: embedded-arduino-dev
description: Arduino 开发全流程指南，覆盖 Arduino IDE / PlatformIO 工程、常用板卡（Uno/Nano/Mega/ESP32/ESP8266）、基础语法、传感器驱动、通信协议、库管理、调试与烧录。用户提及 Arduino、.ino 文件、ESP32、ESP8266、Arduino 库、传感器接线时使用。
---

# Arduino 开发指南

## 概述

本 Skill 面向 Arduino 生态开发，覆盖官方 AVR 板卡（Uno/Nano/Mega）和 ESP 系列（ESP32/ESP8266），提供从环境搭建到项目部署的完整工作流。简单项目使用 Arduino IDE，复杂/多文件项目推荐 PlatformIO。

## 核心规则

- **入口函数**：Arduino 程序必须包含 `setup()`（运行一次）和 `loop()`（循环执行），不要写 `main()`。
- **引脚编号**：使用板卡上丝印的数字编号（如 `D13`、`A0`），不要直接用端口寄存器。
- **库管理**：通过库管理器（Sketch → Include Library → Manage Libraries）安装第三方库，不要手动下载放入 libraries 文件夹（除非库管理器没有）。
- **板卡选择**：烧录前必须在 Tools → Board 中选择正确板卡，ESP32/ESP8266 需先添加附加开发板管理器 URL。
- **避免阻塞**：`loop()` 中不要使用长时间 `delay()`，多用 `millis()` 做非阻塞定时。
- **串口调试**：默认使用 `Serial`（USB 串口），波特率常用 9600 或 115200。

## 快速开始

### Arduino IDE 安装与配置

1. 下载 [Arduino IDE](https://www.arduino.cc/en/software)（推荐 2.x 版本）
2. 安装后打开，Tools → Board → 选择对应板卡（如 Arduino Uno）
3. Tools → Port → 选择串口号（如 COM3）
4. 验证：File → Examples → 01.Basics → Blink → Upload（Ctrl+U）

### ESP32 / ESP8266 支持

```
# ESP32 附加开发板 URL（File → Preferences → Additional boards manager URLs）
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# ESP8266 附加开发板 URL
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

添加后 Tools → Board → Boards Manager → 搜索 "esp32" / "esp8266" → Install。

### PlatformIO（推荐用于复杂项目）

```ini
; platformio.ini 示例
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit BME280 Library@^2.2.2
    bblanchon/ArduinoJson@^7.0.0
```

```bash
# 常用命令
pio run              # 编译
pio run -t upload    # 烧录
pio device monitor   # 串口监视
pio lib install "Adafruit BME280"  # 安装库
```

## 基础语法速查

### 结构

```cpp
void setup() {
    Serial.begin(115200);   // 初始化串口
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
}
```

### GPIO

```cpp
// 输出
pinMode(13, OUTPUT);
digitalWrite(13, HIGH);    // 高电平
digitalWrite(13, LOW);     // 低电平

// 输入
pinMode(2, INPUT);           // 浮空输入
pinMode(2, INPUT_PULLUP);    // 内部上拉输入
int val = digitalRead(2);    // 读取（HIGH/LOW）

// 模拟输入（ADC，10 位：0-1023）
int sensor = analogRead(A0);

// 模拟输出（PWM，仅支持 PWM 引脚，0-255）
analogWrite(9, 128);  // 50% 占空比
```

### 非阻塞定时（替代 delay）

```cpp
unsigned long previousMillis = 0;
const long interval = 1000;

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}
```

### 串口通信

```cpp
Serial.begin(115200);
Serial.print("Hello ");          // 不换行
Serial.println("World");          // 换行
Serial.printf("Value: %d\n", x); // ESP32 支持 printf
Serial.write(0x55);               // 发送原始字节

// 读取
if (Serial.available()) {
    char c = Serial.read();
    String line = Serial.readStringUntil('\n');
}
```

### 中断

```cpp
volatile int counter = 0;

void IRAM_ATTR isr() {    // ESP32 中断函数加 IRAM_ATTR
    counter++;
}

void setup() {
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), isr, FALLING);
    // 模式：RISING / FALLING / CHANGE / LOW / HIGH
}
```

## 常用传感器驱动

### DHT11 / DHT22（温湿度）

```cpp
#include <DHT.h>
DHT dht(2, DHT22);  // 引脚 2，型号 DHT22

void setup() {
    dht.begin();
}
void loop() {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    Serial.printf("Temp: %.1f°C, Humidity: %.1f%%\n", t, h);
    delay(2000);
}
```

### BME280（温湿度+气压）

```cpp
#include <Wire.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;

void setup() {
    bme.begin(0x76);  // 或 0x77
}
void loop() {
    Serial.printf("Temp: %.1f°C, Humidity: %.1f%%, Pressure: %.1fhPa\n",
                  bme.readTemperature(), bme.readHumidity(), bme.readPressure() / 100.0F);
    delay(1000);
}
```

### HC-SR04（超声波测距）

```cpp
const int trigPin = 9, echoPin = 10;

float readDistance() {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH);
    return duration * 0.0343 / 2;  // cm
}
```

### MPU6050（六轴加速度+陀螺仪）

```cpp
#include <Wire.h>
#include <Adafruit_MPU6050.h>
Adafruit_MPU6050 mpu;

void setup() {
    mpu.begin();
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
}
void loop() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    Serial.printf("Accel: (%.2f, %.2f, %.2f) g\n", a.acceleration.x/9.81, a.acceleration.y/9.81, a.acceleration.z/9.81);
}
```

### OLED 显示屏（SSD1306，I2C）

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Hello Arduino!");
    display.display();
}
```

## 通信协议

### I2C

```cpp
#include <Wire.h>
Wire.begin();                    // 主机模式
Wire.beginTransmission(0x68);    // 从机地址
Wire.write(0x3B);                 // 寄存器地址
Wire.endTransmission();
Wire.requestFrom(0x68, 6);       // 请求 6 字节
while (Wire.available()) {
    uint8_t b = Wire.read();
}
```

### SPI

```cpp
#include <SPI.h>
SPI.begin();
digitalWrite(SS, LOW);
SPI.transfer(0x01);  // 发送并接收一字节
digitalWrite(SS, HIGH);
```

### ESP32 WiFi

```cpp
#include <WiFi.h>
const char* ssid = "your-ssid";
const char* password = "your-password";

void setup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(WiFi.localIP());
}
```

### ESP32 HTTP 请求

```cpp
#include <HTTPClient.h>
HTTPClient http;
http.begin("https://api.example.com/data");
int code = http.GET();
if (code > 0) {
    String payload = http.getString();
    Serial.println(payload);
}
http.end();
```

### ESP32 MQTT

```cpp
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("Message [%s]: ", topic);
    for (int i = 0; i < length; i++) Serial.print((char)payload[i]);
    Serial.println();
}

void setup() {
    client.setServer("broker.example.com", 1883);
    client.setCallback(callback);
}
void reconnect() {
    while (!client.connected()) {
        if (client.connect("ESP32Client")) {
            client.subscribe("topic/in");
        } else {
            delay(5000);
        }
    }
}
void loop() {
    if (!client.connected()) reconnect();
    client.loop();
    client.publish("topic/out", "hello");
}
```

## 常用库推荐

| 用途 | 库名 | 说明 |
|------|------|------|
| JSON 解析 | ArduinoJson | 轻量高效，支持序列化/反序列化 |
| 时间同步 | NTPClient | 通过 NTP 获取网络时间 |
| 文件系统 | LittleFS / SPIFFS | ESP32 内置 Flash 文件系统 |
| Web 服务器 | WebServer / ESPAsyncWebServer | 搭建嵌入式 Web 界面 |
| 物联网平台 | ArduinoIoTCloud / PubSubClient | 接入云平台 |
| 显示屏 | Adafruit_GFX + 对应驱动 | OLED/TFT/ePaper 通用图形库 |
| 电机驱动 | AccelStepper | 步进电机平滑控制 |
| PID 控制 | PID | 标准 PID 控制算法 |

## 调试技巧

### 串口打印调试

```cpp
#define DEBUG 1
#if DEBUG
  #define DBG(x) Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x)
  #define DBGLN(x)
#endif
```

### ESP32 异常解码

```bash
# 编译时保留 .elf 文件，异常后解码
pio run -t envdump
# 使用 EspExceptionDecoder 工具解析调用栈
```

### 内存监控（ESP32）

```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
```

## 常见问题排查

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 找不到串口 | 驱动未安装或数据线仅充电 | 安装 CH340/CP2102 驱动；换数据线 |
| 烧录失败（ESP32） | 未进入下载模式 | 按住 BOOT 键再点 Upload，或自动下载电路 |
| 串口乱码 | 波特率不匹配 | Serial Monitor 波特率与 `Serial.begin()` 一致 |
| 模拟读数跳动 | 电源噪声或引脚浮空 | 加滤波电容；使用 `INPUT_PULLUP` |
| 程序跑飞 | 数组越界或栈溢出 | 检查数组边界；ESP32 增大栈 |
| WiFi 连不上 | 密码错误或信号弱 | 确认 2.4G WiFi（ESP32 不支持 5G）；靠近路由器 |
| I2C 设备找不到 | 地址错误或接线问题 | 用 I2C Scanner 扫描地址；检查 SDA/SCL |

## 参考资源

- [Arduino 官方文档](https://docs.arduino.cc/)
- [ESP32 Arduino 核心](https://github.com/espressif/arduino-esp32)
- [PlatformIO 文档](https://docs.platformio.org/)
- [Awesome Arduino](https://github.com/Lembed/Awesome-arduino)
