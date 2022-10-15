#include "PulseOximeter.hpp"

std::string const PulseOximeter::_mqttPubHR = "health-monitor/vitals/HR";
std::string const PulseOximeter::_mqttPubSpO2 = "health-monitor/vitals/SpO2";

PulseOximeter::PulseOximeter(AsyncMqttClient &mqttClient)
{
    _mqttClient = &mqttClient;
}

bool PulseOximeter::begin(TwoWire &wirePort)
{
    if (!_max30102.begin(wirePort, I2C_SPEED_FAST)) // 400kHz speed
    {
        Serial.println("MAX30102 was not found");
        return false;
    }

    byte ledBrightness = 60;   // Options: 0=Off to 255=50mA
    byte sampleAverage = 4;    // Options: 1, 2, 4, 8, 16, 32
    byte ledMode = 2;          // Options: 2 = Red + IR
    uint16_t sampleRate = 200; // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    uint16_t pulseWidth = 411; // Options: 69, 118, 215, 411
    uint16_t adcRange = 4096;  // Options: 2048, 4096, 8192, 16384

    // Configure sensor with these settings
    _max30102.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

    return true;
}

bool PulseOximeter::start()
{
    return xTaskCreate(_loop, "Pulse Oximeter Loop", 2048, static_cast<void *>(this), 2, &_loopTask);
}

void PulseOximeter::stop() const
{
    if (_loopTask != nullptr)
    {
        vTaskDelete(_loopTask);
    }
}

void PulseOximeter::_loop(void *params) // Static member function
{
    PulseOximeter *pulseOx = static_cast<PulseOximeter *>(params);
    byte i;
    float ratio, correl;

    for (;;)
    {
        Serial.println("Reading first samples");

        for (i = 0; i < BUFFER_SIZE; i++) // read the first {BUFFER_SIZE} samples (5 seconds of samples)
        {
            pulseOx->_readMAX30102Sample(i);
            // Serial.printf("%u red=%u\tir=%u\n", i, pulseOx->redBuffer[i], pulseOx->irBuffer[i]);

            if (pulseOx->irBuffer[i] <= _fingerPresenceThreshold) // Checking the IR value to know if there's a finger on the sensor
                break;
        }

        // Continuously taking samples from MAX30102. Heart rate and SpO2 are calculated every 1.25 seconds
        while (pulseOx->irBuffer[BUFFER_SIZE - 1] > _fingerPresenceThreshold) // Check the last IR value before calculating
        {
            // calculate heart rate and SpO2 using Robert Fraczkiewicz's algorithm
            rf_heart_rate_and_oxygen_saturation(
                pulseOx->irBuffer, BUFFER_SIZE, pulseOx->redBuffer,
                &pulseOx->spo2, &pulseOx->validSPO2, &pulseOx->heartRate, &pulseOx->validHeartRate, &ratio, &correl);

            pulseOx->_publishData();
            pulseOx->_printData();

            // dumping the first {FS} sets of samples in the memory and shift the last {BUFFER_SIZE - FS} sets of samples to the top
            for (i = FS; i < BUFFER_SIZE; i++)
            {
                pulseOx->redBuffer[i - FS] = pulseOx->redBuffer[i];
                pulseOx->irBuffer[i - FS] = pulseOx->irBuffer[i];
            }

            for (i = BUFFER_SIZE - FS; i < BUFFER_SIZE; i++) // take {FS} sets of samples before calculating the heart rate and SpO2
            {
                pulseOx->_readMAX30102Sample(i);
                // Serial.printf("red=%u\tir=%u\n", redBuffer[i], irBuffer[i]);
            }
        }

        Serial.println("No finger detected");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void PulseOximeter::_readMAX30102Sample(byte i)
{
    while (!_max30102.available()) // do we have new data?
        _max30102.check();         // Check the sensor for new data

    redBuffer[i] = _max30102.getRed();
    irBuffer[i] = _max30102.getIR();
    _max30102.nextSample(); // We're finished with this sample so move to next sample
}

void PulseOximeter::_printData() const
{
    Serial.printf(
        "HRvalid= %d, HR= %d\tSPO2Valid= %d, SPO2= %.4f\n",
        validHeartRate, heartRate, validSPO2, spo2);
}

void PulseOximeter::_publishData()
{
    if (_mqttClient != nullptr)
    {
        if (validHeartRate)
        {
            _mqttClient->publish(_mqttPubHR.c_str(), 2, true, std::to_string(heartRate).c_str());
        }
        if (validSPO2)
        {
            _mqttClient->publish(_mqttPubHR.c_str(), 2, true, std::to_string(spo2).c_str());
        }
    }
}
