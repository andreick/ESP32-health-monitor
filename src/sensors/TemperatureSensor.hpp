#include <memory>
#include <DallasTemperature.h>
#include <AsyncMqttClient.h>

class TemperatureSensor
{
public:
    TemperatureSensor(uint8_t oneWireBus, AsyncMqttClient &mqttClient);

    void begin();
    bool start();
    bool stop();

private:
    static std::string const _mqttPubTempC;

    AsyncMqttClient *_mqttClient;

    std::unique_ptr<OneWire> _oneWire;
    std::unique_ptr<DallasTemperature> _dallasSensors;

    TimerHandle_t _temperatureTimer;

    float _temperatureC;

    static void _run(TimerHandle_t timer);

    void _readTemperature();
    void _printTemperature() const;
    void _publishTemperature();
};
