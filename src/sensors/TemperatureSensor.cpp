#include "TemperatureSensor.hpp"

std::string const TemperatureSensor::_mqttPubTempC = "health-monitor/vitals/tempC";

TemperatureSensor::TemperatureSensor(uint8_t oneWireBus, AsyncMqttClient &mqttClient)
{
    _oneWire.reset(new OneWire(oneWireBus));
    _dallasSensors.reset(new DallasTemperature(_oneWire.get()));
    _mqttClient = &mqttClient;
}

void TemperatureSensor::begin()
{
    _dallasSensors->begin();
}

bool TemperatureSensor::start()
{
    return xTaskCreate(_loop, "Temperature Sensor Loop", 2048, static_cast<void *>(this), 2, &_loopTask);
}

void TemperatureSensor::stop()
{
    if (_loopTask != nullptr)
    {
        vTaskDelete(_loopTask);
    }
}

void TemperatureSensor::_loop(void *params)
{
    auto *tempSensor = static_cast<TemperatureSensor *>(params);
    TickType_t lastTicks = xTaskGetTickCount();
    for (;;)
    {
        // Run every 1 second
        vTaskDelayUntil(&lastTicks, 1000 / portTICK_PERIOD_MS);

        tempSensor->_readTemperature();
        tempSensor->_publishTemperature();
        tempSensor->_printTemperature();
    }
}

void TemperatureSensor::_readTemperature()
{
    _dallasSensors->requestTemperatures();
    _temperatureC = _dallasSensors->getTempCByIndex(0);
}

void TemperatureSensor::_printTemperature() const
{
    Serial.printf("Celsius temperature= %.4f°C\n", _temperatureC);
}

void TemperatureSensor::_publishTemperature()
{
    if (_mqttClient != nullptr)
    {
        if (_temperatureC != -127.0F)
        {
            _mqttClient->publish(_mqttPubTempC.c_str(), 2, true, String(_temperatureC).c_str());
        }
    }
}
