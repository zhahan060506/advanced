#pragma once
#include "config.h"
#include <cstddef>

namespace board_pin_validation {
template <std::size_t Count>
constexpr bool UniqueAndUsbFree(const int (&pins)[Count]) {
    for (std::size_t i = 0; i < Count; ++i) {
        if (pins[i] < 0) continue;
        if (pins[i] == 19 || pins[i] == 20) return false;
        for (std::size_t j = i + 1; j < Count; ++j) {
            if (pins[i] == pins[j]) return false;
        }
    }
    return true;
}
constexpr int kPins[] = {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
    AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_DIN,
    AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
#else
    AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_DIN, AUDIO_I2S_GPIO_DOUT,
#endif
    BUILTIN_LED_GPIO, BOOT_BUTTON_GPIO, KEY1_BUTTON_GPIO, KEY2_BUTTON_GPIO,
    DISPLAY_MOSI_PIN, DISPLAY_CLK_PIN, DISPLAY_DC_PIN, DISPLAY_RST_PIN,
    DISPLAY_CS_PIN, DISPLAY_BACKLIGHT_PIN, LAMP_GPIO,
    GESTURE_I2C_SDA_GPIO, GESTURE_I2C_SCL_GPIO, STATUS_RING_GPIO, SERVO_GPIO,
};
static_assert(UniqueAndUsbFree(kPins), "Board GPIO conflict or reserved USB pin used");
static_assert(STATUS_RING_LED_COUNT >= 1 && STATUS_RING_LED_COUNT <= 64);
static_assert(STATUS_RING_BRIGHTNESS >= 1 && STATUS_RING_BRIGHTNESS <= 32);
static_assert(GESTURE_POLL_PERIOD_MS >= 20);
}  // namespace board_pin_validation
