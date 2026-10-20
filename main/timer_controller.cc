#include "timing_policy.h"
#include <esp_timer.h>
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

bool TimerController::EnsureWorker() {
    if (worker_started_) return true;
    if (xTaskCreate([](void* arg) {
        static_cast<TimerController*>(arg)->Run();
    }, "timer_controller", 4096, this, 3, nullptr) != pdPASS) return false;
    worker_started_ = true;
    return true;
}

void TimerController::QueueCountdownView(int seconds, uint32_t generation, bool show_page) {
    Application::GetInstance().Schedule([this, seconds, generation, show_page]() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation_ != generation || state_ != kCounting || alarm_state_ == kAlarmRinging) return;
        auto* display = Board::GetInstance().GetDisplay();
        display->SetTimerRemaining(seconds);
        if (show_page && display->GetCurrentPage() != PAGE_TIMER) display->Switch_Timer_Page();
    });
}

void TimerController::QueueHome(uint32_t generation, uint32_t alarm_generation) {
    Application::GetInstance().ReturnToMainPage([this, generation, alarm_generation]() {
        std::lock_guard<std::mutex> lock(mutex_);
        return generation_ == generation && alarm_generation_ == alarm_generation &&
               state_ == kIdle && alarm_state_ != kAlarmRinging;
    });
}

bool TimerController::Start(int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!EnsureWorker()) return false;
    seconds = std::clamp(seconds, 1, 3600);
    const bool was_ringing = state_ == kRinging;
    const auto generation = ++generation_;
    state_ = kCounting;
    countdown_deadline_us_ = esp_timer_get_time() + int64_t{seconds} * 1000000;
    last_remaining_ = seconds;
    if (was_ringing) {
        Application::GetInstance().Schedule([this, generation]() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation_ == generation && alarm_state_ != kAlarmRinging) {
                Application::GetInstance().GetAudioService().ResetDecoder();
            }
        });
    }
    QueueCountdownView(seconds, generation_, true);
    ESP_LOGI(TAG, "Started countdown: %d seconds", seconds);
    return true;
}

void TimerController::Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool ringing = state_ == kRinging;
    state_ = kIdle;
    const auto generation = ++generation_;
    if (ringing) {
        Application::GetInstance().Schedule([this, generation]() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (generation_ == generation && alarm_state_ != kAlarmRinging) {
                Application::GetInstance().GetAudioService().ResetDecoder();
            }
        });
    }
    QueueHome(generation_, alarm_generation_);
    ESP_LOGI(TAG, "Timer cancelled");
}

bool TimerController::SetAlarm(int hour, int minute) {
    hour = std::clamp(hour, 0, 23);
    minute = std::clamp(minute, 0, 59);
    const time_t now = time(nullptr);
    struct tm target_tm = {};
    localtime_r(&now, &target_tm);
    if (target_tm.tm_year < 2025 - 1900) return false;
    target_tm.tm_hour = hour;
    target_tm.tm_min = minute;
    target_tm.tm_sec = 0;
    target_tm.tm_isdst = -1;
    time_t target = mktime(&target_tm);
    if (target <= now) {
        ++target_tm.tm_mday;
        target_tm.tm_isdst = -1;
        target = mktime(&target_tm);
    }
    if (target == static_cast<time_t>(-1)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!EnsureWorker()) return false;
    const auto generation = ++alarm_generation_;
    alarm_target_ = target;
    alarm_state_ = kAlarmScheduled;
    Application::GetInstance().Schedule([this, generation, hour, minute]() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (alarm_generation_ != generation || alarm_state_ != kAlarmScheduled) return;
        char message[48];
        std::snprintf(message, sizeof(message), "闹钟已设置 %02d:%02d", hour, minute);
        Board::GetInstance().GetDisplay()->ShowNotification(message);
    });
    ESP_LOGI(TAG, "Alarm set: %02d:%02d", hour, minute);
    return true;
}

void TimerController::CancelAlarm() {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool ringing = alarm_state_ == kAlarmRinging;
    alarm_state_ = kAlarmIdle;
    const auto generation = ++alarm_generation_;
    if (ringing) {
        Application::GetInstance().Schedule([this, generation]() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (alarm_generation_ == generation && state_ != kRinging) {
                Application::GetInstance().GetAudioService().ResetDecoder();
            }
        });
        if (state_ == kCounting) {
            last_remaining_ = -1;
        } else {
            QueueHome(generation_, alarm_generation_);
        }
    }
    ESP_LOGI(TAG, "Alarm cancelled");
}

bool TimerController::HandleKey1() {
    const auto state = GetIndicatorState();
    if (state.ringing) {
        // Stop both sounding sources on one press, preserving a future alarm.
        bool countdown_ringing;
        bool alarm_ringing;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            countdown_ringing = state_ == kRinging;
            alarm_ringing = alarm_state_ == kAlarmRinging;
        }
        if (countdown_ringing) Cancel();
        if (alarm_ringing) CancelAlarm();
        return true;
    }
    if (state.counting) { Cancel(); return true; }
    return false;
}

bool TimerController::IsBusy() const {
    const auto state = GetIndicatorState();
    return state.counting || state.ringing;
}

std::string TimerController::GetAlarmText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (alarm_state_ == kAlarmRinging) return "闹钟响铃中";
    if (alarm_state_ == kAlarmIdle) return "尚未设置闹钟\n前推进入对话后\n说：设置七点的闹钟";
    struct tm target = {};
    localtime_r(&alarm_target_, &target);
    char text[96];
    snprintf(text, sizeof(text), "下次闹钟\n%02d-%02d  %02d:%02d", target.tm_mon + 1,
             target.tm_mday, target.tm_hour, target.tm_min);
    return text;
}

TimerController::IndicatorState TimerController::GetIndicatorState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {state_ == kCounting, state_ == kRinging || alarm_state_ == kAlarmRinging};
}

void TimerController::Run() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const int64_t now = esp_timer_get_time();
            bool finished = false;
            bool new_ringing = false;
            if (state_ == kCounting) {
                const int remaining = timing_policy::CountdownSeconds(countdown_deadline_us_, now);
                if (remaining == 0) {
                    state_ = kRinging;
                    countdown_ring_end_us_ = now + 5000000;
                    new_ringing = true;
                } else if (remaining != last_remaining_) {
                    const bool restore_page = last_remaining_ < 0;
                    last_remaining_ = remaining;
                    QueueCountdownView(remaining, generation_, restore_page);
                }
            }
            if (alarm_state_ == kAlarmScheduled && time(nullptr) >= alarm_target_) {
                alarm_state_ = kAlarmRinging;
                alarm_ring_end_us_ = now + 30000000;
                new_ringing = true;
            }
            if (state_ == kRinging && now >= countdown_ring_end_us_) {
                state_ = kIdle;
                finished = true;
            }
            if (alarm_state_ == kAlarmRinging && now >= alarm_ring_end_us_) {
                alarm_state_ = kAlarmIdle;
                last_remaining_ = -1;
                finished = true;
            }
            const bool alarm = alarm_state_ == kAlarmRinging;
            const bool ringing = alarm || state_ == kRinging;
            if (new_ringing) { next_tone_us_ = now; tone_index_ = 0; }
            if (ringing && now >= next_tone_us_) {
                next_tone_us_ = now + 1000000;
                const int frequency = alarm ? (tone_index_ % 2 ? 1319 : 988) : (tone_index_ % 2 ? 1175 : 880);
                ++tone_index_;
                const auto generation = alarm ? alarm_generation_ : generation_;
                Application::GetInstance().Schedule([this, alarm, generation, frequency, new_ringing]() {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (alarm ? (alarm_generation_ != generation || alarm_state_ != kAlarmRinging)
                              : (generation_ != generation || state_ != kRinging || alarm_state_ == kAlarmRinging)) return;
                    auto* display = Board::GetInstance().GetDisplay();
                    if (alarm) display->SetTimerText("闹钟");
                    else display->SetTimerRemaining(0);
                    display->Switch_Timer_Page();
                    auto& audio = Application::GetInstance().GetAudioService();
                    if (new_ringing) audio.ResetDecoder();
                    audio.PlayTone(frequency, 350, alarm ? 75 : 70);
                });
            }
            if (finished && !ringing && state_ == kIdle) QueueHome(generation_, alarm_generation_);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
