#include "status_light_controller.h"

#include "device_state_event.h"
#include "status_light_policy.h"
#include "timer_controller.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <led_strip.h>
#include <soc/soc_caps.h>

namespace {
constexpr const char* kTag = "StatusLight";
constexpr uint32_t kErrorDurationMs = 1500;
constexpr uint32_t kTestColorDurationMs = 350;

uint32_t NowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

const char* ModeName(status_light::Mode mode) {
    using status_light::Mode;
    switch (mode) {
        case Mode::kOff: return "standby/off";
        case Mode::kBoot: return "starting/blue blink";
        case Mode::kWifiConfiguring: return "wifi setup/blue blink";
        case Mode::kConnecting: return "connecting/cyan";
        case Mode::kListening: return "listening/blue";
        case Mode::kSpeaking: return "speaking/green";
        case Mode::kCountdown: return "countdown/yellow";
        case Mode::kRinging: return "ringing/red blink";
        case Mode::kError: return "error/red blink";
        case Mode::kUpgrading: return "upgrading/purple blink";
        case Mode::kActivating: return "activating/cyan blink";
        case Mode::kTestRed: return "test/red";
        case Mode::kTestGreen: return "test/green";
        case Mode::kTestBlue: return "test/blue";
        default: return "unknown";
    }
}
}  // namespace

StatusLightController& StatusLightController::GetInstance() {
    static StatusLightController instance;
    return instance;
}

esp_err_t StatusLightController::Initialize(gpio_num_t pin, int led_count,
        uint8_t brightness, bool startup_test, DeviceState initial_state) {
    if (started_) return ESP_ERR_INVALID_STATE;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || led_count < 1 || led_count > 64 ||
        brightness == 0 || brightness > 32) {
        return ESP_ERR_INVALID_ARG;
    }
    pin_ = pin;
    led_count_ = led_count;
    brightness_ = brightness;
    startup_test_ = startup_test;
    state_.store(initial_state);
    if (!callback_registered_) {
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback(
            [this](DeviceState, DeviceState current) { state_.store(current); });
        callback_registered_ = true;
    }
    started_ = true;
    if (xTaskCreate([](void* arg) {
            static_cast<StatusLightController*>(arg)->Run();
            vTaskDelete(nullptr);
        }, "status_ring", 4096, this, 2, nullptr) != pdPASS) {
        started_ = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void StatusLightController::NotifyError() {
    // The worker owns timing. This is nonblocking even from network callbacks.
    error_sequence_.fetch_add(1);
}

void StatusLightController::Run() {
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = pin_;
    strip_config.max_leds = led_count_;
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;
#if SOC_RMT_SUPPORT_DMA
    rmt_config.flags.with_dma = true;
#endif
    led_strip_handle_t strip = nullptr;
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "RMT init failed (%s); TV continues without ring",
                 esp_err_to_name(err));
        return;
    }
    err = led_strip_clear(strip);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Initial clear failed (%s); ring disabled", esp_err_to_name(err));
        led_strip_del(strip);
        return;
    }
    ESP_LOGI(kTag, "Driver ready: GPIO=%d, LEDs=%d, brightness=%u/255, RGB test=%s",
             pin_, led_count_, brightness_, startup_test_ ? "on" : "off");

    using status_light::Mode;
    const uint32_t startup_ms = NowMs();
    uint32_t mode_start_ms = startup_ms;
    uint32_t error_start_ms = 0;
    uint32_t seen_error_sequence = 0;
    bool error_active = false;
    bool test_done = !startup_test_;
    bool first_frame = true;
    Mode previous_mode = Mode::kOff;
    status_light::Color previous_color{};
    int consecutive_errors = 0;

    while (true) {
        const uint32_t now = NowMs();
        const uint32_t error_sequence = error_sequence_.load();
        if (error_sequence != seen_error_sequence) {
            seen_error_sequence = error_sequence;
            error_start_ms = now;
            error_active = true;
        }
        if (error_active && now - error_start_ms >= kErrorDurationMs) {
            error_active = false;
        }
        const auto timer = TimerController::GetInstance().GetIndicatorState();
        Mode mode = status_light::SelectMode(state_.load(), timer.counting,
                                            timer.ringing, error_active);
        if (!test_done) {
            // Important live states immediately cancel the startup color test.
            if (mode == Mode::kRinging || mode == Mode::kError ||
                mode == Mode::kListening || mode == Mode::kSpeaking ||
                mode == Mode::kCountdown || mode == Mode::kUpgrading ||
                now - startup_ms >= 3 * kTestColorDurationMs) {
                test_done = true;
            } else {
                const uint32_t phase = (now - startup_ms) / kTestColorDurationMs;
                mode = phase == 0 ? Mode::kTestRed :
                       phase == 1 ? Mode::kTestGreen : Mode::kTestBlue;
            }
        }
        if (first_frame || mode != previous_mode) {
            mode_start_ms = now;
            previous_mode = mode;
            ESP_LOGI(kTag, "Mode: %s", ModeName(mode));
        }
        const auto color = status_light::Frame(mode, now - mode_start_ms, brightness_);
        if (first_frame || !(color == previous_color)) {
            err = ESP_OK;
            for (int index = 0; index < led_count_; ++index) {
                err = led_strip_set_pixel(strip, index, color.red, color.green, color.blue);
                if (err != ESP_OK) break;
            }
            if (err == ESP_OK) err = led_strip_refresh(strip);
            if (err != ESP_OK) {
                if (++consecutive_errors == 1) {
                    ESP_LOGW(kTag, "Output failed: %s", esp_err_to_name(err));
                }
                if (consecutive_errors >= 3) {
                    ESP_LOGW(kTag, "Repeated output failures; ring disabled");
                    break;
                }
            } else {
                consecutive_errors = 0;
                previous_color = color;
                first_frame = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    led_strip_clear(strip);
    led_strip_del(strip);
}
