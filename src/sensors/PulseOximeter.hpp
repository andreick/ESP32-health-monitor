#include <MAX30105.h>
#include <algorithm_by_RF.h>
#include <AsyncMqttClient.h>

class PulseOximeter
{
public:
    PulseOximeter(AsyncMqttClient &mqttClient);

    bool begin(TwoWire &wirePort = Wire);
    bool start();
    void stop() const;

private:
    static constexpr uint32_t _fingerPresenceThreshold = 200000;
    static std::string const _mqttPubHR;
    static std::string const _mqttPubSpO2;

    AsyncMqttClient *_mqttClient;

    TaskHandle_t _loopTask;

    MAX30105 _max30102;

    uint32_t irBuffer[BUFFER_SIZE];  // infrared LED sensor data
    uint32_t redBuffer[BUFFER_SIZE]; // red LED sensor data
    float spo2;                      // SPO2 value
    int8_t validSPO2;                // indicator to show if the SPO2 calculation is valid
    int32_t heartRate;               // heart rate value
    int8_t validHeartRate;           // indicator to show if the heart rate calculation is valid

    static void _loop(void *params);

    void _readMAX30102Sample(byte i);

    void _printData() const;
    void _publishData();
};
