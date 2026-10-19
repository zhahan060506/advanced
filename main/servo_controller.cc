#include "servo_controller.h"

#include "device_state_event.h"
#include "mcp_server.h"
#include "servo_motion.h"
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr const char* kTag = "Servo";
// Backlight uses timer 0 / channel 0. Reserve these for this board's servo.
constexpr ledc_timer_t kTimer = LEDC_TIMER_2;
constexpr ledc_channel_t kChannel = LEDC_CHANNEL_2;
constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;
uint32_t NowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void ReleasePwm() {
    ledc_stop(kMode, kChannel, 0);
    ledc_timer_pause(kMode, kTimer);
    ledc_timer_config_t config = {};
    config.speed_mode = kMode;
    config.timer_num = kTimer;
    config.deconfigure = true;
    ledc_timer_config(&config);
}
}  // namespace

ServoController& ServoController::GetInstance() {
    static ServoController instance;
    return instance;
}

esp_err_t ServoController::Initialize(gpio_num_t pin, bool startup_test,
        bool follow_speech, DeviceState initial_state) {
    if (initialized_) return ESP_ERR_INVALID_STATE;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) return ESP_ERR_INVALID_ARG;
    ledc_timer_config_t timer = {};
    timer.speed_mode = kMode;
    timer.timer_num = kTimer;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.freq_hz = 50;
    timer.clk_cfg = LEDC_AUTO_CLK;
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;

    ledc_channel_config_t channel = {};
    channel.gpio_num = pin;
    channel.speed_mode = kMode;
    channel.channel = kChannel;
    channel.timer_sel = kTimer;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.duty = 0;  // No pulses until the worker starts.
    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        ReleasePwm();
        return err;
    }
    startup_test_ = startup_test;
    follow_speech_ = follow_speech;
    state_.store(initial_state);
    if (follow_speech_ && !callback_registered_) {
        DeviceStateEventManager::GetInstance().RegisterStateChangeCallback(
            [this](DeviceState, DeviceState state) { state_.store(state); });
        callback_registered_ = true;
    }
    available_.store(true);
    if (xTaskCreate([](void* arg) {
            static_cast<ServoController*>(arg)->Run();
            vTaskDelete(nullptr);
        }, "servo_motion", 3072, this, 2, nullptr) != pdPASS) {
        available_.store(false);
        ReleasePwm();
        return ESP_ERR_NO_MEM;
    }
    initialized_ = true;
    ESP_LOGI(kTag, "PWM configured: GPIO=%d, 50Hz, logical range=60..120, test=%d, speech=%d",
             pin, startup_test_, follow_speech_);
    return ESP_OK;
}

bool ServoController::SetAngle(int angle) {
    if (angle < servo_motion::kMinAngle || angle > servo_motion::kMaxAngle ||
        !available_.load() || stop_.load()) return false;
    target_.store(angle);
    // A manual command cancels startup test and the speech overlay until reboot.
    manual_requested_.store(true);
    return true;
}

void ServoController::Stop() {
    stop_.store(true);
    available_.store(false);
}

void ServoController::RegisterMcpTools() {
    if (tools_registered_ || !available_.load()) return;
    tools_registered_ = true;
    auto& server = McpServer::GetInstance();
    server.AddTool("self.servo.set_angle",
        "Move the screen servo to a logical angle from 60 to 120; 90 is center. "
        "Use only when the user requests screen rotation or centering. "
        "Physical left/right depends on installation. Cancels automatic motion until reboot. "
        "A queued command does not confirm physical movement.",
        PropertyList({Property("angle", kPropertyTypeInteger, 60, 120)}),
        [this](const PropertyList& properties) -> ReturnValue {
            return std::string(SetAngle(properties["angle"].value<int>())
                ? "servo movement queued; physical position is not measured"
                : "servo unavailable, stopped, or angle out of range");
        });
    server.AddTool("self.servo.stop",
        "Stop servo PWM output until reboot when the user requests stopping the servo. "
        "Does not disconnect servo power or guarantee that it holds its position.",
        PropertyList(), [this](const PropertyList&) -> ReturnValue {
            Stop();
            return std::string("servo PWM stop requested; reboot required to re-enable");
        });
}

void ServoController::Run() {
    int current = servo_motion::kCenter;
    int last_written = -1;
    const uint32_t start = NowMs();
    uint32_t speaking_start = start;
    bool was_speaking = false;
    bool test_active = startup_test_;
    while (!stop_.load()) {
        const uint32_t now = NowMs();
        const DeviceState state = state_.load();
        if (follow_speech_ && state == kDeviceStateFatalError) break;
        const bool manual = manual_requested_.load();
        const bool speaking = follow_speech_ && state == kDeviceStateSpeaking && !manual;
        if (speaking && !was_speaking) speaking_start = now;
        was_speaking = speaking;
        if (manual || speaking || now - start >= 4000) test_active = false;
        int target = target_.load();
        if (test_active) target = servo_motion::TestTarget(now - start);
        else if (speaking) target = servo_motion::SpeakingTarget(target, now - speaking_start);
        current = servo_motion::Step(current, target);
        if (last_written != current) {
            esp_err_t err = ledc_set_duty(kMode, kChannel, servo_motion::Duty(current));
            if (err == ESP_OK) err = ledc_update_duty(kMode, kChannel);
            if (err != ESP_OK) {
                ESP_LOGW(kTag, "PWM output failed (%s); servo stopped", esp_err_to_name(err));
                break;
            }
            last_written = current;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    available_.store(false);
    stop_.store(true);
    ReleasePwm();
    ESP_LOGW(kTag, "PWM stopped; reboot required to re-enable");
}
