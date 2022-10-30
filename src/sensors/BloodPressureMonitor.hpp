#include <Wire.h>
#include <AsyncMqttClient.h>
#include "abstract/LoopTask.hpp"
#include "util/millis.hpp"

class BloodPressureMonitor : public LoopTask
{
public:
    BloodPressureMonitor(uint8_t powerPin, AsyncMqttClient &mqttClient);

    void begin(TwoWire &wirePort, int sda, int scl);
    bool start();

private:
    static std::string const _mqttPubBP;

    uint8_t _powerPin;

    static void _loop(void *params);
    void _pushPowerButton();

    static void _receiveEvent(int numBytes);
    static int _readData();
    static String _hexToString(int hex);
    static void _printData();
    static void _publishBloodPressure();
};
