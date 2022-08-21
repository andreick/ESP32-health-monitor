#include <Wire.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

#define WIFI_TIMEOUT 15000

const char *ssidPath = "/wifi-ssid.txt";
const char *passPath = "/wifi-pass.txt";

MAX30105 particleSensor;

uint32_t irBuffer[100];  // infrared LED sensor data
uint32_t redBuffer[100]; // red LED sensor data
int32_t spo2;            // SPO2 value
int8_t validSPO2;        // indicator to show if the SPO2 calculation is valid
int32_t heartRate;       // heart rate value
int8_t validHeartRate;   // indicator to show if the heart rate calculation is valid

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

void initParticleSensor()
{
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
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

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); // Configure sensor with these settings
}

void setup()
{
    Serial.begin(115200);
    initSPIFFS();
    connectToWifi();
    initParticleSensor();
}

void loop()
{
    for (byte i = 0; i < 100; i++) // read the first 100 samples, and determine the signal range
    {
        while (!particleSensor.available()) // do we have new data?
            particleSensor.check();         // Check the sensor for new data

        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample(); // We're finished with this sample so move to next sample

        Serial.print("red=");
        Serial.print(redBuffer[i], DEC);
        Serial.print(", ir=");
        Serial.println(irBuffer[i], DEC);
    }

    // calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
    maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    for (;;) // Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
    {
        Serial.print("HR=");
        Serial.print(heartRate, DEC);
        Serial.print(", HRvalid=");
        Serial.print(validHeartRate, DEC);
        Serial.print(", SPO2=");
        Serial.print(spo2, DEC);
        Serial.print(", SPO2Valid=");
        Serial.println(validSPO2, DEC);

        for (byte i = 25; i < 100; i++) // dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
        {
            redBuffer[i - 25] = redBuffer[i];
            irBuffer[i - 25] = irBuffer[i];
        }

        for (byte i = 75; i < 100; i++) // take 25 sets of samples before calculating the heart rate.
        {
            while (!particleSensor.available()) // do we have new data?
                particleSensor.check();         // Check the sensor for new data

            redBuffer[i] = particleSensor.getRed();
            irBuffer[i] = particleSensor.getIR();
            particleSensor.nextSample(); // We're finished with this sample so move to next sample
        }

        // After gathering 25 new samples recalculate HR and SP02
        maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    }
}
