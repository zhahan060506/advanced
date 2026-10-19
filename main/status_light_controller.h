#ifndef STATUS_LIGHT_CONTROLLER_H_
#define STATUS_LIGHT_CONTROLLER_H_

#include <atomic>
#include <cstdint>
#include <driver/gpio.h>
#include <esp_err.h>
#include "device_state.h"

// Board-lifetime singleton. One worker owns all RMT/strip operations.
class StatusLightController {
public:
    static StatusLightController& GetInstance();
    StatusLightController(const StatusLightController&) = delete;
    StatusLightController& operator=(const StatusLightController&) = delete;

    // Called once by board startup. Missing physical LEDs cannot be detected.
    esp_err_t Initialize(gpio_num_t pin, int led_count, uint8_t brightness,
                         bool startup_test, DeviceState initial_state);
    void NotifyError();

private:
    StatusLightController() = default;
    void Run();

    gpio_num_t pin_ = GPIO_NUM_NC;
    int led_count_ = 0;
    uint8_t brightness_ = 24;
    bool startup_test_ = false;
    bool started_ = false;
    bool callback_registered_ = false;
    std::atomic<DeviceState> state_{kDeviceStateUnknown};
    std::atomic<uint32_t> error_sequence_{0};
};

#endif  // STATUS_LIGHT_CONTROLLER_H_
