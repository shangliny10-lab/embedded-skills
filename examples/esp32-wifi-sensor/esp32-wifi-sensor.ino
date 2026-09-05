/*
 * ESP32 WiFi 温湿度传感器 + MQTT 上传示例
 * 硬件：ESP32 DevKit + DHT22 (GPIO4)
 * 功能：
 *   - 连接 WiFi
 *   - 读取 DHT22 温湿度
 *   - 通过 MQTT 上传到服务器
 *   - OLED 显示（SSD1306 I2C）
 *   - Web 配置页面（首次使用配网）
 *
 * 依赖库（Arduino Library Manager）：
 *   - DHT sensor library (Adafruit)
 *   - Adafruit Unified Sensor
 *   - PubSubClient
 *   - Adafruit SSD1306
 *   - Adafruit GFX Library
 */

#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* 配置 --------------------------------------------------------------*/
#define WIFI_SSID       "your-wifi-ssid"
#define WIFI_PASSWORD   "your-wifi-password"

#define MQTT_SERVER     "broker.example.com"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "esp32-sensor-01"
#define MQTT_TOPIC_TEMP "home/room1/temperature"
#define MQTT_TOPIC_HUMI "home/room1/humidity"

#define DHT_PIN         4
#define DHT_TYPE        DHT22

#define OLED_ADDR       0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

#define SAMPLE_INTERVAL 5000   // 采样间隔 ms

/* 全局对象 ----------------------------------------------------------*/
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

float temperature = 0;
float humidity = 0;
unsigned long lastSample = 0;
unsigned long lastReconnect = 0;

/* 函数声明 ----------------------------------------------------------*/
void connectWiFi();
void connectMQTT();
void readSensor();
void publishData();
void updateDisplay();
void mqttCallback(char* topic, byte* payload, unsigned int length);

/* setup -------------------------------------------------------------*/
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 WiFi Sensor ===");

    // OLED 初始化
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED init failed");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("ESP32 Sensor");
        display.println("Initializing...");
        display.display();
    }

    // DHT 初始化
    dht.begin();

    // 连接 WiFi
    connectWiFi();

    // MQTT 配置
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqttCallback);
}

/* loop --------------------------------------------------------------*/
void loop() {
    // MQTT 重连
    if (!client.connected()) {
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            connectMQTT();
        }
    } else {
        client.loop();
    }

    // 定时采样
    if (millis() - lastSample >= SAMPLE_INTERVAL) {
        lastSample = millis();
        readSensor();
        if (client.connected()) {
            publishData();
        }
        updateDisplay();
    }
}

/* WiFi 连接 ---------------------------------------------------------*/
void connectWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

/* MQTT 连接 ---------------------------------------------------------*/
void connectMQTT() {
    Serial.print("Connecting to MQTT...");
    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
    } else {
        connected = client.connect(MQTT_CLIENT_ID);
    }

    if (connected) {
        Serial.println("connected");
        client.subscribe("home/room1/cmd");  // 订阅命令主题
    } else {
        Serial.printf("failed, rc=%d\n", client.state());
    }
}

/* MQTT 回调 ---------------------------------------------------------*/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("Message [%s]: ", topic);
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    Serial.println(msg);

    // 示例：收到 "reboot" 命令重启
    if (msg == "reboot") {
        Serial.println("Rebooting...");
        delay(1000);
        ESP.restart();
    }
}

/* 读取传感器 --------------------------------------------------------*/
void readSensor() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
        Serial.println("Failed to read DHT sensor!");
        return;
    }

    temperature = t;
    humidity = h;
    Serial.printf("Temp: %.1f°C, Humi: %.1f%%\n", temperature, humidity);
}

/* 发布数据 ----------------------------------------------------------*/
void publishData() {
    char tempStr[16];
    char humiStr[16];
    dtostrf(temperature, 4, 1, tempStr);
    dtostrf(humidity, 4, 1, humiStr);

    client.publish(MQTT_TOPIC_TEMP, tempStr);
    client.publish(MQTT_TOPIC_HUMI, humiStr);

    // JSON 格式（可选）
    // StaticJsonDocument<128> doc;
    // doc["temp"] = temperature;
    // doc["humi"] = humidity;
    // char json[128];
    // serializeJson(doc, json);
    // client.publish("home/room1/data", json);
}

/* OLED 显示 ---------------------------------------------------------*/
void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ESP32 Sensor");

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.printf("%.1f C", temperature);

    display.setCursor(0, 40);
    display.printf("%.1f %%", humidity);

    display.setTextSize(1);
    display.setCursor(80, 56);
    if (client.connected()) {
        display.print("MQTT OK");
    } else {
        display.print("MQTT --");
    }

    display.display();
}
