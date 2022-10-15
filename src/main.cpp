#include <WiFiManager.h>
#include <AsyncMqttClient.h>
#include <DallasTemperature.h>

#include "sensors/PulseOximeter.hpp"

#define MQTT_HOST IPAddress(142, 93, 53, 68)
#define MQTT_PORT 1883

#define MQTT_PUB_TEMPC "health-monitor/temperature-celsius"

#define ONE_WIRE_BUS 18

AsyncMqttClient mqttClient;
PulseOximeter pulseOximeter(mqttClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
TimerHandle_t tempTimer;
float temperatureC;

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

void readTemperature()
{
    tempSensors.requestTemperatures();
    temperatureC = tempSensors.getTempCByIndex(0);
}

void printTemperature()
{
    Serial.printf("Celsius temperature= %.4f°C\n", temperatureC);
}

void publishTemperature()
{
    if (temperatureC != -127.0F)
    {
        mqttClient.publish(MQTT_PUB_TEMPC, 2, true, String(temperatureC).c_str());
    }
}

void runTempSensor(TimerHandle_t timer)
{
    readTemperature();
    publishTemperature();
    printTemperature();
}

void initTempSensor()
{
    tempSensors.begin();
    tempTimer = xTimerCreate("Temperature timer", 1000 / portTICK_PERIOD_MS, pdTRUE, (void *)0, runTempSensor);
    xTimerStart(tempTimer, 0);
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
    initTempSensor();

    vTaskDelete(nullptr); // Delete loop task
}

void loop() {} // Execution never reach here
