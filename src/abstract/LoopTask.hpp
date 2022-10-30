#ifndef ABSTRACT_LOOPTASK_HPP
#define ABSTRACT_LOOPTASK_HPP
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class LoopTask
{
public:
    virtual bool start() = 0;
    void stop();

protected:
    TaskHandle_t _loopHandle;
};

#endif
