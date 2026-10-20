#ifndef TIMER_CONTROLLER_H
#define TIMER_CONTROLLER_H

#include <atomic>
#include <mutex>
#include <cstdint>
#include <ctime>
#include <string>

class TimerController {
public:
    static TimerController& GetInstance();

    void RegisterMcpTools();
    bool Start(int seconds);
    void Cancel();
    bool SetAlarm(int hour, int minute);
    void CancelAlarm();
    bool HandleKey1();
    bool IsBusy() const;
    std::string GetAlarmText() const;

    struct IndicatorState {
        bool counting;
        bool ringing;
    };
    IndicatorState GetIndicatorState() const;

private:
    enum TimerState {
        kIdle = 0,
        kCounting,
        kRinging,
    };

    enum AlarmState {
        kAlarmIdle = 0,
        kAlarmScheduled,
        kAlarmRinging,
    };

    TimerController() = default;
    TimerController(const TimerController&) = delete;
    TimerController& operator=(const TimerController&) = delete;

    bool EnsureWorker();  // Caller holds mutex_.
    void Run();
    void QueueCountdownView(int seconds, uint32_t generation, bool show_page = false);
    void QueueHome(uint32_t generation, uint32_t alarm_generation);
    mutable std::mutex mutex_;
    bool worker_started_ = false;
    TimerState state_ = kIdle;
    AlarmState alarm_state_ = kAlarmIdle;
    uint32_t generation_ = 0;
    uint32_t alarm_generation_ = 0;
    int64_t countdown_deadline_us_ = 0;
    int64_t countdown_ring_end_us_ = 0;
    int64_t alarm_ring_end_us_ = 0;
    int64_t next_tone_us_ = 0;
    int last_remaining_ = -1;
    unsigned tone_index_ = 0;
    time_t alarm_target_ = 0;
};

#endif
