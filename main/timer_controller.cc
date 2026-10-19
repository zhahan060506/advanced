#include "timer_controller.h"

#include "application.h"
#include "board.h"
#include "display.h"
#include "mcp_server.h"

#include <algorithm>
#include <cstdio>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <ctime>

#define TAG "TimerController"

namespace {
struct TimerTaskArgs {
    int seconds;
    int generation;
};

struct AlarmTaskArgs {
    time_t target_time;
    int generation;
};
} // namespace

TimerController& TimerController::GetInstance() {
    static TimerController instance;
    return instance;
}

void TimerController::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.timer.start",
        "Start a countdown timer on the device. Convert minutes to seconds before calling this tool. "
        "Use this when the user asks to set, start, or create a countdown timer.",
        PropertyList({
            Property("seconds", kPropertyTypeInteger, 1, 3600)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            int seconds = properties["seconds"].value<int>();
            bool started = TimerController::GetInstance().Start(seconds);
            if (!started) {
                return std::string("failed to start timer");
            }
            return std::string("timer started");
        });

    mcp_server.AddTool("self.timer.cancel",
        "Cancel the current countdown timer or stop the timer alarm.",
        PropertyList(),
        [](const PropertyList&) -> ReturnValue {
            TimerController::GetInstance().Cancel();
            return std::string("timer cancelled");
        });

    mcp_server.AddTool("self.alarm.set",
        "Set an alarm on the device using local 24-hour clock time. "
        "Use this when the user asks to set an alarm clock, such as 'set an alarm for 7:30'. "
        "If the requested time today has already passed, the alarm will ring tomorrow.",
        PropertyList({
            Property("hour", kPropertyTypeInteger, 0, 23),
            Property("minute", kPropertyTypeInteger, 0, 59)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            int hour = properties["hour"].value<int>();
            int minute = properties["minute"].value<int>();
            bool started = TimerController::GetInstance().SetAlarm(hour, minute);
            if (!started) {
                return std::string("failed to set alarm");
            }
            return std::string("alarm set");
        });

    mcp_server.AddTool("self.alarm.cancel",
        "Cancel the scheduled alarm or stop the alarm ringing.",
        PropertyList(),
        [](const PropertyList&) -> ReturnValue {
            TimerController::GetInstance().CancelAlarm();
            return std::string("alarm cancelled");
        });
}

bool TimerController::Start(int seconds) {
    seconds = std::clamp(seconds, 1, 3600);
    int generation = generation_.fetch_add(1) + 1;
    state_.store(kCounting);

    auto display = Board::GetInstance().GetDisplay();
    display->SetTimerRemaining(seconds);
    display->Switch_Timer_Page();

    auto args = new TimerTaskArgs {
        .seconds = seconds,
        .generation = generation,
    };

    BaseType_t ret = xTaskCreate([](void* arg) {
        auto args = static_cast<TimerTaskArgs*>(arg);
        auto& timer = TimerController::GetInstance();
        auto display = Board::GetInstance().GetDisplay();

        for (int remaining = args->seconds; remaining > 0; --remaining) {
            if (timer.generation_.load() != args->generation || timer.state_.load() != kCounting) {
                delete args;
                vTaskDelete(nullptr);
                return;
            }
            display->SetTimerRemaining(remaining);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (timer.generation_.load() == args->generation && timer.state_.load() == kCounting) {
            display->SetTimerRemaining(0);
            timer.state_.store(kRinging);
            ESP_LOGI(TAG, "Timer finished, ringing for 5 seconds");
            Application::GetInstance().GetAudioService().ResetDecoder();

            for (int i = 0; i < 5; ++i) {
                if (timer.generation_.load() != args->generation || timer.state_.load() != kRinging) {
                    delete args;
                    vTaskDelete(nullptr);
                    return;
                }
                Application::GetInstance().GetAudioService().PlayTone((i % 2 == 0) ? 880 : 1175, 350, 70);
                vTaskDelay(pdMS_TO_TICKS(650));
            }

            if (timer.generation_.load() == args->generation && timer.state_.load() == kRinging) {
                timer.state_.store(kIdle);
                Application::GetInstance().ReturnToMainPage();
            }
        }

        delete args;
        vTaskDelete(nullptr);
    }, "timer_countdown", 4096, args, 3, nullptr);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create timer task");
        delete args;
        state_.store(kIdle);
        return false;
    }

    ESP_LOGI(TAG, "Started countdown: %d seconds", seconds);
    return true;
}

void TimerController::Cancel() {
    int previous_state = state_.exchange(kIdle);
    generation_.fetch_add(1);

    if (previous_state == kRinging) {
        Application::GetInstance().GetAudioService().ResetDecoder();
    }

    Application::GetInstance().ReturnToMainPage();
    ESP_LOGI(TAG, "Timer cancelled");
}

bool TimerController::SetAlarm(int hour, int minute) {
    hour = std::clamp(hour, 0, 23);
    minute = std::clamp(minute, 0, 59);

    time_t now = time(nullptr);
    struct tm now_tm = {};
    localtime_r(&now, &now_tm);
    if (now_tm.tm_year < 2025 - 1900) {
        ESP_LOGW(TAG, "Cannot set alarm before system time is ready");
        return false;
    }

    struct tm target_tm = now_tm;
    target_tm.tm_hour = hour;
    target_tm.tm_min = minute;
    target_tm.tm_sec = 0;
    time_t target_time = mktime(&target_tm);
    if (target_time <= now) {
        target_time += 24 * 60 * 60;
    }

    int generation = alarm_generation_.fetch_add(1) + 1;
    alarm_state_.store(kAlarmScheduled);

    auto args = new AlarmTaskArgs {
        .target_time = target_time,
        .generation = generation,
    };

    BaseType_t ret = xTaskCreate([](void* arg) {
        auto args = static_cast<AlarmTaskArgs*>(arg);
        auto& timer = TimerController::GetInstance();

        while (true) {
            if (timer.alarm_generation_.load() != args->generation ||
                timer.alarm_state_.load() != kAlarmScheduled) {
                delete args;
                vTaskDelete(nullptr);
                return;
            }

            time_t now = time(nullptr);
            if (now >= args->target_time) {
                break;
            }

            int wait_seconds = static_cast<int>(args->target_time - now);
            int delay_seconds = std::clamp(wait_seconds, 1, 10);
            vTaskDelay(pdMS_TO_TICKS(delay_seconds * 1000));
        }

        if (timer.alarm_generation_.load() == args->generation &&
            timer.alarm_state_.load() == kAlarmScheduled) {
            auto display = Board::GetInstance().GetDisplay();
            display->SetTimerText("闹钟");
            display->Switch_Timer_Page();
            timer.alarm_state_.store(kAlarmRinging);
            ESP_LOGI(TAG, "Alarm ringing for 30 seconds");
            Application::GetInstance().GetAudioService().ResetDecoder();

            for (int i = 0; i < 30; ++i) {
                if (timer.alarm_generation_.load() != args->generation ||
                    timer.alarm_state_.load() != kAlarmRinging) {
                    delete args;
                    vTaskDelete(nullptr);
                    return;
                }
                Application::GetInstance().GetAudioService().PlayTone((i % 2 == 0) ? 988 : 1319, 350, 75);
                vTaskDelay(pdMS_TO_TICKS(650));
            }

            if (timer.alarm_generation_.load() == args->generation &&
                timer.alarm_state_.load() == kAlarmRinging) {
                timer.alarm_state_.store(kAlarmIdle);
                Application::GetInstance().ReturnToMainPage();
            }
        }

        delete args;
        vTaskDelete(nullptr);
    }, "alarm_clock", 4096, args, 3, nullptr);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create alarm task");
        delete args;
        alarm_state_.store(kAlarmIdle);
        return false;
    }

    char message[48];
    std::snprintf(message, sizeof(message), "闹钟已设置 %02d:%02d", hour, minute);
    Board::GetInstance().GetDisplay()->ShowNotification(message);
    ESP_LOGI(TAG, "Alarm set: %02d:%02d", hour, minute);
    return true;
}

void TimerController::CancelAlarm() {
    int previous_state = alarm_state_.exchange(kAlarmIdle);
    alarm_generation_.fetch_add(1);

    if (previous_state == kAlarmRinging) {
        Application::GetInstance().GetAudioService().ResetDecoder();
        Application::GetInstance().ReturnToMainPage();
    }

    ESP_LOGI(TAG, "Alarm cancelled");
}

bool TimerController::HandleKey1() {
    if (state_.load() != kIdle) {
        Cancel();
        return true;
    }

    if (alarm_state_.load() == kAlarmRinging) {
        CancelAlarm();
        return true;
    }

    return false;
}

bool TimerController::IsBusy() const {
    return state_.load() != kIdle || alarm_state_.load() == kAlarmRinging;
}

TimerController::IndicatorState TimerController::GetIndicatorState() const {
    const int timer_state = state_.load();
    const int alarm_state = alarm_state_.load();
    return {timer_state == kCounting,
            timer_state == kRinging || alarm_state == kAlarmRinging};
}
