#pragma once

#include <atomic>
#include <driver/gpio.h>
#include <esp_err.h>
#include "device_state.h"

// Board-lifetime singleton. The worker is the sole writer of PWM after init.
class ServoController {
public:
    static ServoController& GetInstance();
    esp_err_t Initialize(gpio_num_t pin, bool startup_test, bool follow_speech,
                         DeviceState initial_state);
    bool SetAngle(int logical_angle);
    bool Center() { return SetAngle(90); }
    void Stop();  // Latched until reboot; removing PWM does not remove power.
    void RegisterMcpTools();

private:
    ServoController() = default;
    void Run();
    bool startup_test_ = false;
    bool follow_speech_ = false;
    bool initialized_ = false;
    bool callback_registered_ = false;
    bool tools_registered_ = false;
    std::atomic<bool> available_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> manual_requested_{false};
    std::atomic<int> target_{90};
    std::atomic<DeviceState> state_{kDeviceStateUnknown};
};
