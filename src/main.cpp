#include <WiFiManager.h>
#include <AsyncMqttClient.h>
#include <MAX30105.h>
#include <spo2_algorithm.h>
#include <DallasTemperature.h>

#define MQTT_HOST IPAddress(142, 93, 53, 68)
#define MQTT_PORT 1883

#define MQTT_PUB_HR "health-monitor/heart-rate"
#define MQTT_PUB_SPO2 "health-monitor/oxygen-saturation"
#define MQTT_PUB_TEMPC "health-monitor/temperature-celsius"

#define ONE_WIRE_BUS 18

AsyncMqttClient mqttClient;

MAX30105 max30102;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

uint32_t irBuffer[100];  // infrared LED sensor data
uint32_t redBuffer[100]; // red LED sensor data
int32_t spo2;            // SPO2 value
int8_t validSPO2;        // indicator to show if the SPO2 calculation is valid
int32_t heartRate;       // heart rate value
int8_t validHeartRate;   // indicator to show if the heart rate calculation is valid
float temperatureC;

void restart(uint32_t ms)
{
    Serial.printf("Restarting in %u milliseconds...", ms);
    delay(ms);
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
        delay(1000);
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

void initMAX30102()
{
    if (!max30102.begin(Wire, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
    {
        Serial.println("MAX30105 was not found");
        restart(5000);
    }

    byte ledBrightness = 60;   // Options: 0=Off to 255=50mA
    byte sampleAverage = 4;    // Options: 1, 2, 4, 8, 16, 32
    byte ledMode = 2;          // Options: 2 = Red + IR
    byte sampleRate = 100;     // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    uint16_t pulseWidth = 411; // Options: 69, 118, 215, 411
    uint16_t adcRange = 4096;  // Options: 2048, 4096, 8192, 16384

    max30102.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); // Configure sensor with these settings
}

void setup()
{
    Serial.begin(115200);

    if (!initWifiManager())
    {
        Serial.println("WiFi connection failed");
        restart(5000);
    }

    initMqtt();
    initMAX30102();
    tempSensors.begin();
}

void readMAX30102Sample(byte i)
{
    while (!max30102.available()) // do we have new data?
        max30102.check();         // Check the sensor for new data

    redBuffer[i] = max30102.getRed();
    irBuffer[i] = max30102.getIR();
}

void readTemperature()
{
    tempSensors.requestTemperatures();
    temperatureC = tempSensors.getTempCByIndex(0);
}

void printSensorsData()
{
    Serial.printf(
        "HRvalid= %d, HR= %d, SPO2Valid= %d, SPO2= %d, Celsius temperature= %.4f°C\n",
        validHeartRate, heartRate, validSPO2, spo2, temperatureC);
}

void publishSensorsData()
{
    mqttClient.publish(MQTT_PUB_HR, 2, true, String(heartRate).c_str());
    mqttClient.publish(MQTT_PUB_SPO2, 2, true, String(spo2).c_str());
    mqttClient.publish(MQTT_PUB_TEMPC, 2, true, String(temperatureC).c_str());
}

void loop()
{
    for (byte i = 0; i < 100; i++) // read the first 100 samples, and determine the signal range
    {
        readMAX30102Sample(i);
        max30102.nextSample(); // We're finished with this sample so move to next sample

        Serial.print("red=");
        Serial.print(redBuffer[i]);
        Serial.print(", ir=");
        Serial.println(irBuffer[i]);
    }

    // calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
    maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    for (;;) // Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
    {
        readTemperature();
        printSensorsData();
        publishSensorsData();

        for (byte i = 25; i < 100; i++) // dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
        {
            redBuffer[i - 25] = redBuffer[i];
            irBuffer[i - 25] = irBuffer[i];
        }

        for (byte i = 75; i < 100; i++) // take 25 sets of samples before calculating the heart rate.
        {
            readMAX30102Sample(i);
            max30102.nextSample(); // We're finished with this sample so move to next sample
        }

        // After gathering 25 new samples recalculate HR and SP02
        maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    }
}
