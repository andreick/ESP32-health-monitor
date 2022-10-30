#include <memory>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncMqttClient.h>
#include "abstract/LoopTask.hpp"

class TemperatureSensor : public LoopTask
{
public:
    TemperatureSensor(uint8_t oneWireBus, AsyncMqttClient &mqttClient);

    void begin();
    bool start();

private:
    static std::string const _mqttPubTempC;

    AsyncMqttClient *_mqttClient;

    std::unique_ptr<OneWire> _oneWire;
    std::unique_ptr<DallasTemperature> _dallasSensors;

    float _temperatureC;

    static void _loop(void *params);

    void _readTemperature();
    void _printTemperature() const;
    void _publishTemperature();
};
