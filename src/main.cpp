#include <WiFiManager.h>
#include <AsyncMqttClient.h>
#include <MAX30105.h>
#include <algorithm_by_RF.h>
#include <DallasTemperature.h>

#define MQTT_HOST IPAddress(142, 93, 53, 68)
#define MQTT_PORT 1883

#define MQTT_PUB_HR "health-monitor/heart-rate"
#define MQTT_PUB_SPO2 "health-monitor/oxygen-saturation"
#define MQTT_PUB_TEMPC "health-monitor/temperature-celsius"

#define FINGER_PRESENCE_THRESHOLD 200000

#define ONE_WIRE_BUS 18

AsyncMqttClient mqttClient;

MAX30105 max30102;
uint32_t irBuffer[BUFFER_SIZE];  // infrared LED sensor data
uint32_t redBuffer[BUFFER_SIZE]; // red LED sensor data
float spo2;                      // SPO2 value
int8_t validSPO2;                // indicator to show if the SPO2 calculation is valid
int32_t heartRate;               // heart rate value
int8_t validHeartRate;           // indicator to show if the heart rate calculation is valid

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

void readMAX30102Sample(byte i)
{
    while (!max30102.available()) // do we have new data?
        max30102.check();         // Check the sensor for new data

    redBuffer[i] = max30102.getRed();
    irBuffer[i] = max30102.getIR();
    max30102.nextSample(); // We're finished with this sample so move to next sample
}

void printMAX30102Data()
{
    Serial.printf(
        "HRvalid= %d, HR= %d\tSPO2Valid= %d, SPO2= %.4f\n",
        validHeartRate, heartRate, validSPO2, spo2);
}

void publishMAX30102Data()
{
    if (validHeartRate)
    {
        mqttClient.publish(MQTT_PUB_HR, 2, true, String(heartRate).c_str());
    }
    if (validSPO2)
    {
        mqttClient.publish(MQTT_PUB_SPO2, 2, true, String(spo2).c_str());
    }
}

void runMAX30102(void *params)
{
    byte i;
    float ratio, correl;

    for (;;)
    {
        Serial.println("Reading first samples");

        for (i = 0; i < BUFFER_SIZE; i++) // read the first {BUFFER_SIZE} samples (5 seconds of samples)
        {
            readMAX30102Sample(i);
            Serial.printf("%u red=%u\tir=%u\n", i, redBuffer[i], irBuffer[i]);

            if (irBuffer[i] <= FINGER_PRESENCE_THRESHOLD) // Checking the IR value to know if there's a finger on the sensor
                break;
        }

        // Continuously taking samples from MAX30102. Heart rate and SpO2 are calculated every 1.25 seconds
        while (irBuffer[BUFFER_SIZE - 1] > FINGER_PRESENCE_THRESHOLD) // Check the last IR value before calculating
        {
            // calculate heart rate and SpO2 using Robert Fraczkiewicz's algorithm
            rf_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate, &ratio, &correl);

            publishMAX30102Data();
            printMAX30102Data();

            // dumping the first {FS} sets of samples in the memory and shift the last {BUFFER_SIZE - FS} sets of samples to the top
            for (i = FS; i < BUFFER_SIZE; i++)
            {
                redBuffer[i - FS] = redBuffer[i];
                irBuffer[i - FS] = irBuffer[i];
            }

            for (i = BUFFER_SIZE - FS; i < BUFFER_SIZE; i++) // take {FS} sets of samples before calculating the heart rate and SpO2
            {
                readMAX30102Sample(i);
                // Serial.printf("red=%u\tir=%u\n", redBuffer[i], irBuffer[i]);
            }
        }

        Serial.println("No finger detected");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void initMAX30102()
{
    if (!max30102.begin(Wire, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
    {
        Serial.println("MAX30102 was not found");
        restart(5);
    }

    byte ledBrightness = 60;   // Options: 0=Off to 255=50mA
    byte sampleAverage = 4;    // Options: 1, 2, 4, 8, 16, 32
    byte ledMode = 2;          // Options: 2 = Red + IR
    uint16_t sampleRate = 200; // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    uint16_t pulseWidth = 411; // Options: 69, 118, 215, 411
    uint16_t adcRange = 4096;  // Options: 2048, 4096, 8192, 16384

    max30102.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); // Configure sensor with these settings

    xTaskCreate(runMAX30102, "Run MAX30102", 2048, NULL, 2, NULL);
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
    initMAX30102();
    initTempSensor();

    vTaskDelete(NULL); // Delete loop task
}

void loop() {} // Execution never reach here
