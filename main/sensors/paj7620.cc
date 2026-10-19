#include "paj7620.h"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr const char* kTag = "PAJ7620";
constexpr int kTransferTimeoutMs = 100;
constexpr uint8_t kBankSelect = 0xEF;

// Register initialization adapted from Seeed-Studio/Grove_Gesture,
// src/paj7620.h (June 2023). Full attribution: paj7620_LICENSE.txt.
// https://github.com/Seeed-Studio/Grove_Gesture/blob/master/src/paj7620.h
constexpr uint8_t kInitRegisters[][2] = {
    // Bank 0.
    {0xEF,0x00}, {0x37,0x07}, {0x38,0x17}, {0x39,0x06}, {0x42,0x01},
    {0x46,0x2D}, {0x47,0x0F}, {0x48,0x3C}, {0x49,0x00}, {0x4A,0x1E},
    {0x4C,0x20}, {0x51,0x10}, {0x5E,0x10}, {0x60,0x27}, {0x80,0x42},
    {0x81,0x44}, {0x82,0x04}, {0x8B,0x01}, {0x90,0x06}, {0x95,0x0A},
    {0x96,0x0C}, {0x97,0x05}, {0x9A,0x14}, {0x9C,0x3F}, {0xA5,0x19},
    {0xCC,0x19}, {0xCD,0x0B}, {0xCE,0x13}, {0xCF,0x64}, {0xD0,0x21},
    // Bank 1.
    {0xEF,0x01}, {0x02,0x0F}, {0x03,0x10}, {0x04,0x02}, {0x25,0x01},
    {0x27,0x39}, {0x28,0x7F}, {0x29,0x08}, {0x3E,0xFF}, {0x5E,0x3D},
    {0x65,0x96}, {0x67,0x97}, {0x69,0xCD}, {0x6A,0x01}, {0x6D,0x2C},
    {0x6E,0x01}, {0x72,0x01}, {0x73,0x35}, {0x77,0x01}, {0xEF,0x00},
};
}  // namespace

Paj7620::Paj7620(i2c_master_bus_handle_t bus, uint8_t address)
    : bus_(bus), address_(address) {}

Paj7620::~Paj7620() {
    if (device_ != nullptr) {
        i2c_master_bus_rm_device(device_);
    }
}

esp_err_t Paj7620::WriteRegister(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device_, data, sizeof(data), kTransferTimeoutMs);
}

esp_err_t Paj7620::ReadRegister(uint8_t reg, uint8_t& value) {
    return i2c_master_transmit_receive(device_, &reg, 1, &value, 1,
                                     kTransferTimeoutMs);
}

esp_err_t Paj7620::Initialize() {
    if (bus_ == nullptr || device_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    // Allow power-on settling before probing an optional peripheral.
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_err_t err = i2c_master_probe(bus_, address_, kTransferTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "No response at 0x%02X (%s); check power, SDA and SCL",
                 address_, esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = address_;
    device_config.scl_speed_hz = 100000;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_, &device_config, &device_),
                        kTag, "Cannot add I2C device");

    // Seeed's wake sequence, then explicitly restore bank 0 after MCU resets.
    ESP_RETURN_ON_ERROR(WriteRegister(0xFF, 0x00), kTag, "Wake failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(WriteRegister(kBankSelect, 0), kTag, "Bank select failed");
    uint8_t id_low = 0, id_high = 0;
    ESP_RETURN_ON_ERROR(ReadRegister(0x00, id_low), kTag, "ID read failed");
    ESP_RETURN_ON_ERROR(ReadRegister(0x01, id_high), kTag, "ID read failed");
    if (id_low != 0x20 || id_high != 0x76) {
        ESP_LOGW(kTag, "Unexpected chip ID 0x%02X%02X; expected 0x7620",
                 id_high, id_low);
        return ESP_ERR_INVALID_RESPONSE;
    }

    for (const auto& entry : kInitRegisters) {
        ESP_RETURN_ON_ERROR(WriteRegister(entry[0], entry[1]), kTag,
                            "Init register 0x%02X failed", entry[0]);
    }
    // Same NEAR_240FPS mode as Seeed's reference driver.
    ESP_RETURN_ON_ERROR(WriteRegister(kBankSelect, 1), kTag, "Bank select failed");
    ESP_RETURN_ON_ERROR(WriteRegister(0x65, 0x12), kTag, "Report mode failed");
    ESP_RETURN_ON_ERROR(WriteRegister(kBankSelect, 0), kTag, "Bank select failed");
    ESP_RETURN_ON_ERROR(WriteRegister(0x41, 0xFF), kTag, "Gesture mask failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t pending = 0;
    ESP_RETURN_ON_ERROR(ReadRegister(0x44, pending), kTag, "Clear flags failed");
    ESP_RETURN_ON_ERROR(ReadRegister(0x43, pending), kTag, "Clear flags failed");
    initialized_ = true;
    ESP_LOGI(kTag, "Ready: address=0x%02X, chip=0x7620, mode=NEAR_240FPS", address_);
    return ESP_OK;
}

esp_err_t Paj7620::ReadGesture(Gesture& gesture, uint16_t& raw_flags) {
    gesture = Gesture::kNone;
    raw_flags = 0;
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    // Match the reference driver's high-then-low read order. Do not read twice
    // after decoding: that could discard a gesture arriving between samples.
    uint8_t high = 0, low = 0;
    esp_err_t err = ReadRegister(0x44, high);
    if (err != ESP_OK) {
        return err;
    }
    err = ReadRegister(0x43, low);
    if (err != ESP_OK) {
        return err;
    }
    raw_flags = (static_cast<uint16_t>(high) << 8) | low;
    // Direction convention follows Seeed's June 2023 library. Physical module
    // orientation must be checked before adding page/volume control.
    switch (raw_flags) {
        case 0x000: gesture = Gesture::kNone; break;
        case 0x001: gesture = Gesture::kUp; break;
        case 0x002: gesture = Gesture::kDown; break;
        case 0x004: gesture = Gesture::kLeft; break;
        case 0x008: gesture = Gesture::kRight; break;
        case 0x010: gesture = Gesture::kForward; break;
        case 0x020: gesture = Gesture::kBackward; break;
        case 0x040: gesture = Gesture::kClockwise; break;
        case 0x080: gesture = Gesture::kCounterClockwise; break;
        case 0x100: gesture = Gesture::kWave; break;
        default: gesture = Gesture::kUnknown; break;
    }
    return ESP_OK;
}

const char* Paj7620::GestureName(Gesture gesture) {
    switch (gesture) {
        case Gesture::kNone: return "NONE";
        case Gesture::kUp: return "UP";
        case Gesture::kDown: return "DOWN";
        case Gesture::kLeft: return "LEFT";
        case Gesture::kRight: return "RIGHT";
        case Gesture::kForward: return "FORWARD";
        case Gesture::kBackward: return "BACKWARD";
        case Gesture::kClockwise: return "CLOCKWISE";
        case Gesture::kCounterClockwise: return "COUNTER_CLOCKWISE";
        case Gesture::kWave: return "WAVE";
        default: return "UNKNOWN";
    }
}
