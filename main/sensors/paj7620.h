#ifndef PAJ7620_H_
#define PAJ7620_H_

#include <cstdint>
#include "gesture_policy.h"
#include <driver/i2c_master.h>

// Single-task driver. The caller owns the I2C bus and must outlive this device.
class Paj7620 {
public:
    using Gesture = gesture_policy::Gesture;

    explicit Paj7620(i2c_master_bus_handle_t bus, uint8_t address = 0x73);
    ~Paj7620();
    Paj7620(const Paj7620&) = delete;
    Paj7620& operator=(const Paj7620&) = delete;

    esp_err_t Initialize();
    // Reads/clears hardware flags. Multiple flags remain UNKNOWN for diagnostics;
    // phase one deliberately does not turn ambiguous flags into control actions.
    esp_err_t ReadGesture(Gesture& gesture, uint16_t& raw_flags);
    static const char* GestureName(Gesture gesture);

private:
    esp_err_t WriteRegister(uint8_t reg, uint8_t value);
    esp_err_t ReadRegister(uint8_t reg, uint8_t& value);

    i2c_master_bus_handle_t bus_;
    uint8_t address_;
    i2c_master_dev_handle_t device_ = nullptr;
    bool initialized_ = false;
};

#endif  // PAJ7620_H_
