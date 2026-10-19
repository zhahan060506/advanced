#ifndef TIMER_CONTROLLER_H
#define TIMER_CONTROLLER_H

#include <atomic>

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

    std::atomic<int> state_ {kIdle};
    std::atomic<int> generation_ {0};
    std::atomic<int> alarm_state_ {kAlarmIdle};
    std::atomic<int> alarm_generation_ {0};
};

#endif
