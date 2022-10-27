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
    _temperatureTimer = xTimerCreate("Temperature timer", 1000 / portTICK_PERIOD_MS,
                                     pdTRUE, static_cast<void *>(this), _run);
}

bool TemperatureSensor::start()
{
    if (_temperatureTimer == nullptr)
        return false;
    return xTimerStart(_temperatureTimer, 0);
}

bool TemperatureSensor::stop()
{
    if (_temperatureTimer == nullptr)
        return false;
    return xTimerStop(_temperatureTimer, 0);
}

void TemperatureSensor::_run(TimerHandle_t timer)
{
    auto *tempSensor = static_cast<TemperatureSensor *>(pvTimerGetTimerID(timer));
    tempSensor->_readTemperature();
    tempSensor->_publishTemperature();
    tempSensor->_printTemperature();
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
    if (_temperatureC != -127.0F)
    {
        _mqttClient->publish(_mqttPubTempC.c_str(), 2, true, String(_temperatureC).c_str());
    }
}
