#include <WiFi.h>
#include <SPIFFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <MAX30105.h>
#include "spo2_algorithm.h"

#define ONE_WIRE_BUS 18

#define WIFI_TIMEOUT 15000

const char *ssidPath = "/wifi-ssid.txt";
const char *passPath = "/wifi-pass.txt";

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
MAX30105 max30102;

uint32_t irBuffer[100];  // infrared LED sensor data
uint32_t redBuffer[100]; // red LED sensor data
int32_t spo2;            // SPO2 value
int8_t validSPO2;        // indicator to show if the SPO2 calculation is valid
int32_t heartRate;       // heart rate value
int8_t validHeartRate;   // indicator to show if the heart rate calculation is valid
float temperatureC;
float temperatureF;

void restart()
{
    Serial.print("Restarting in 5 seconds...");
    delay(5000);
    ESP.restart();
}

void initSPIFFS()
{
    if (!SPIFFS.begin())
    {
        restart();
    }
}

String readFile(fs::FS &fs, const char *path)
{
    Serial.print("Reading file: ");
    Serial.println(path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("- failed to open file for reading");
        return String();
    }

    String fileContent;
    while (file.available())
    {
        fileContent = file.readStringUntil('\n');
        break;
    }
    return fileContent;
}

void connectToWifi()
{
    String ssid = readFile(SPIFFS, ssidPath);
    String pass = readFile(SPIFFS, passPath);

    WiFi.mode(WIFI_STA);
    WiFi.begin(&ssid[0], &pass[0]);

    Serial.print("Connecting to WiFi");
    unsigned long startAttemptMs = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startAttemptMs > WIFI_TIMEOUT)
        {
            Serial.println("\r\nConnection failed");
            restart();
        }
        Serial.print(".");
        delay(100);
    }

    Serial.print("Connected on IP: ");
    Serial.println(WiFi.localIP());
}

void initMAX30102()
{
    if (!max30102.begin(Wire, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
    {
        Serial.println("MAX30105 was not found");
        return;
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
    initSPIFFS();
    connectToWifi();
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
    temperatureF = tempSensors.getTempFByIndex(0);
}

void printSensorsData()
{
    Serial.print(", HRvalid=");
    Serial.print(validHeartRate);
    Serial.print("HR=");
    Serial.print(heartRate);
    Serial.print(", SPO2Valid=");
    Serial.print(validSPO2);
    Serial.print(", SPO2=");
    Serial.print(spo2);
    Serial.print(", Celsius temperature=");
    Serial.print(temperatureC);
    Serial.print("ºC");
    Serial.print(", Fahrenheit temperature=");
    Serial.print(temperatureF);
    Serial.println("ºF");
}

void loop()
{
    for (byte i = 0; i < 100; i++) // read the first 100 samples, and determine the signal range
    {
        readMAX30102Sample(i);
        max30102.nextSample(); // We're finished with this sample so move to next sample

        Serial.print("red=");
        Serial.print(redBuffer[i], DEC);
        Serial.print(", ir=");
        Serial.println(irBuffer[i], DEC);
    }

    // calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
    maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    for (;;) // Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
    {
        readTemperature();
        printSensorsData();

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
