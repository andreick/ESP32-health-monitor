#include <MAX30105.h>
#include <algorithm_by_RF.h>
#include <AsyncMqttClient.h>
#include "abstract/LoopTask.hpp"

class PulseOximeter : public LoopTask
{
public:
    PulseOximeter(AsyncMqttClient &mqttClient);

    bool begin(TwoWire &wirePort, int sda, int scl);
    bool start();

private:
    static constexpr uint32_t _fingerPresenceThreshold = 200000;
    static std::string const _mqttPubHR;
    static std::string const _mqttPubSpO2;

    AsyncMqttClient *_mqttClient;

    MAX30105 _max30102;

    uint32_t _irBuffer[BUFFER_SIZE];  // infrared LED sensor data
    uint32_t _redBuffer[BUFFER_SIZE]; // red LED sensor data
    float _spo2;                      // SPO2 value
    int8_t _validSPO2;                // indicator to show if the SPO2 calculation is valid
    int32_t _heartRate;               // heart rate value
    int8_t _validHeartRate;           // indicator to show if the heart rate calculation is valid

    static void _loop(void *params);

    void _readMAX30102Sample(byte i);

    void _printData() const;
    void _publishData();
};
