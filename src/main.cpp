#include <WiFiManager.h>
#include <AsyncMqttClient.h>

#include "sensors/PulseOximeter.hpp"
#include "sensors/TemperatureSensor.hpp"

#define MQTT_HOST IPAddress(142, 93, 53, 68)
#define MQTT_PORT 1883

constexpr uint8_t oneWireBus = 18;

AsyncMqttClient mqttClient;
PulseOximeter pulseOximeter(mqttClient);
TemperatureSensor tempSensor(oneWireBus, mqttClient);

void restart(byte seconds)
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.printf("Restarting in %u seconds...", seconds);
    delay(seconds * 1000);
    digitalWrite(LED_BUILTIN, LOW);
    ESP.restart();
}

bool initWifiManager()
{
    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    return wm.autoConnect("HEALTH-MONITOR");
}

void connectToMqtt()
{
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
    Serial.println("Connected to MQTT");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Serial.println("MQTT connection failed");
    if (WiFi.isConnected())
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        connectToMqtt();
    }
}

void initMqtt()
{
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    connectToMqtt();
}

void setup()
{
    Serial.begin(115200);

    if (!initWifiManager())
    {
        Serial.println("WiFi connection failed");
        restart(5);
    }

    initMqtt();
    pulseOximeter.begin();
    pulseOximeter.start();
    tempSensor.begin();
    tempSensor.start();

    vTaskDelete(nullptr); // Delete loop task
}

void loop() {} // Execution never reach here
