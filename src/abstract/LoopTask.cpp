#include "LoopTask.hpp"

void LoopTask::stop()
{
    if (_loopHandle != nullptr)
    {
        vTaskDelete(_loopHandle);
    }
}
