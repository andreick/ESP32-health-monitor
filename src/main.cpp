#include <WiFiManager.h>
#include <AsyncMqttClient.h>

#include "sensors/PulseOximeter.hpp"
#include "sensors/TemperatureSensor.hpp"
#include "sensors/BloodPressureMonitor.hpp"

const IPAddress mqttBrokerIp(137, 184, 107, 130);
constexpr uint16_t mqttPort = 1883;

constexpr uint8_t tempOneWireBus = 18;
constexpr int pulseOxSda = 21;
constexpr int pulseOxScl = 22;
constexpr uint8_t bpmPowerPin = 25;
constexpr int bpmSda = 27;
constexpr int bpmScl = 26;

AsyncMqttClient mqttClient;
PulseOximeter pulseOximeter(mqttClient);
TemperatureSensor tempSensor(tempOneWireBus, mqttClient);
BloodPressureMonitor bloodPressureMonitor(bpmPowerPin, mqttClient);

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
    mqttClient.setServer(mqttBrokerIp, mqttPort);
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
    pulseOximeter.begin(Wire1, pulseOxSda, pulseOxScl);
    pulseOximeter.start();
    tempSensor.begin();
    tempSensor.start();
    bloodPressureMonitor.begin(Wire, bpmSda, bpmScl);
    bloodPressureMonitor.start();

    vTaskDelete(nullptr); // Delete loop task
}

void loop() {} // Execution never reach here
