#include "BloodPressureMonitor.hpp"

std::string const BloodPressureMonitor::_mqttPubBP = "health-monitor/vitals/BP";

byte numWritings;
String sys;
String dia;
String bp;
int hr;
AsyncMqttClient *_mqttClient;

BloodPressureMonitor::BloodPressureMonitor(uint8_t powerPin, AsyncMqttClient &mqttClient)
{
    _powerPin = powerPin;
    pinMode(_powerPin, OUTPUT);
    _mqttClient = &mqttClient;
}

void BloodPressureMonitor::begin(TwoWire &wirePort, int sda, int scl)
{
    wirePort.begin(0x50, sda, scl, 400000);
    wirePort.onReceive(_receiveEvent);
}

bool BloodPressureMonitor::start()
{
    return xTaskCreate(_loop, "BPM Loop", configMINIMAL_STACK_SIZE, static_cast<void *>(this), 1, &_loopTask);
}

void BloodPressureMonitor::stop()
{
    if (_loopTask != nullptr)
    {
        vTaskDelete(_loopTask);
    }
}

void BloodPressureMonitor::_loop(void *params)
{
    auto bpm = static_cast<BloodPressureMonitor *>(params);
    TickType_t lastTicks = xTaskGetTickCount();
    for (;;)
    {
        // The start button must be pressed twice to begin the measurement
        bpm->_pushPowerButton();
        vTaskDelay(200 / portTICK_PERIOD_MS);
        bpm->_pushPowerButton();

        // Run every 2 minutes
        vTaskDelayUntil(&lastTicks, 2 * minuteMs / portTICK_PERIOD_MS);
    }
}

void BloodPressureMonitor::_pushPowerButton() // Emulate a push on start button
{
    digitalWrite(_powerPin, HIGH);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    digitalWrite(_powerPin, LOW);
}

void BloodPressureMonitor::_receiveEvent(int numBytes)
{
    if (numBytes == 2) // Receiving a writing
    {
        switch (++numWritings)
        {
        case 1:
        {
            int memAddress = Wire.read();
            if (memAddress == 0xF)
            {
                Serial.println("Measurement error");
                numWritings = 0;
            }
            break;
        }
        case 6:
        {
            int data = _readData();
            switch (data)
            {
            case 0x10:
                sys = "1";
                dia = "";
                break;
            case 0x11:
                sys = "1";
                dia = "1";
                break;
            case 0x01:
                sys = "";
                dia = "1";
                break;
            case 0x00:
                sys = "";
                dia = "";
                break;
            }
            break;
        }
        case 7:
        {
            int data = _readData();
            sys += _hexToString(data);
            break;
        }
        case 8:
        {
            int data = _readData();
            dia += _hexToString(data);
            break;
        }
        case 9:
            hr = _readData();
            break;
        case 10:
            bp = sys + "/" + dia;
            _printData();
            numWritings = 0;
            break;
        }
    }
}

int BloodPressureMonitor::_readData()
{
    Wire.read(); // Discard the memory address
    return Wire.read();
}

String BloodPressureMonitor::_hexToString(int hex)
{
    String str = String(hex, HEX);
    if (str.length() == 1)
    {
        return '0' + str;
    }
    return str;
}

void BloodPressureMonitor::_printData()
{
    Serial.printf("Blood pressure= %s mmHg\tHeart rate= %d bpm\n", bp, hr);
}

void BloodPressureMonitor::_publishData()
{
    if (_mqttClient != nullptr)
    {
        _mqttClient->publish(_mqttPubBP.c_str(), 2, true, bp.c_str());
    }
}
