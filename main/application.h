#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <string>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>

#include "protocol.h"
#include "ota.h"
#include "audio_service.h"
#include "device_state_event.h"
#include "gesture_policy.h"

#define MAIN_EVENT_SCHEDULE (1 << 0)
#define MAIN_EVENT_SEND_AUDIO (1 << 1)
#define MAIN_EVENT_WAKE_WORD_DETECTED (1 << 2)
#define MAIN_EVENT_VAD_CHANGE (1 << 3)
#define MAIN_EVENT_ERROR (1 << 4)
#define MAIN_EVENT_CHECK_NEW_VERSION_DONE (1 << 5)
#define MAIN_EVENT_CLOCK (1 << 6)


enum AecMode {
    kAecOff,
    kAecOnDeviceSide,
    kAecOnServerSide,
};

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    void Start();
    DeviceState GetDeviceState() const { return device_state_.load(); }
    bool IsVoiceDetected() const { return audio_service_.IsVoiceDetected(); }
    void Schedule(std::function<void()> callback);
    void SetDeviceState(DeviceState state);
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    void DismissAlert();
    void AbortSpeaking(AbortReason reason);
    void ToggleChatState();
    void HandleGesture(gesture_policy::Gesture gesture);
    void StartListening();
    void StopListening();
    void ReturnToMainPage(std::function<bool()> condition = {});
    void Reboot();
    void WakeWordInvoke(const std::string& wake_word);
    bool CanEnterSleepMode();
    void SendMcpMessage(const std::string& payload);
    void SetAecMode(AecMode mode);
    AecMode GetAecMode() const { return aec_mode_.load(); }
    void PlaySound(const std::string_view& sound);
    AudioService& GetAudioService() { return audio_service_; }

private:
    std::atomic<bool> gesture_pending_{false};
    Application();
    ~Application();

    std::mutex mutex_;
    std::deque<std::function<void()>> main_tasks_;
    std::unique_ptr<Protocol> protocol_;
    EventGroupHandle_t event_group_ = nullptr;
    esp_timer_handle_t clock_timer_handle_ = nullptr;
    std::atomic<DeviceState> device_state_{kDeviceStateUnknown};
    std::atomic<int64_t> chat_idle_start_us_{0};
    ListeningMode listening_mode_ = kListeningModeAutoStop;
    std::atomic<AecMode> aec_mode_{kAecOff};
    std::mutex error_mutex_;
    std::string last_error_message_;
    AudioService audio_service_;

    bool has_server_time_ = false;
    bool aborted_ = false;
    std::atomic<int> clock_ticks_{0};
    TaskHandle_t check_new_version_task_handle_ = nullptr;
    TaskHandle_t healthTaskHandle = nullptr;

    void MainEventLoop();
    void OnWakeWordDetected();
    void CheckNewVersion(Ota& ota);
    void ShowActivationCode(const std::string& code, const std::string& message);
    void OnClockTimer();
    void OnClockTick();
    void SetListeningMode(ListeningMode mode);
    void EnterHomeStandby();
};

#endif // _APPLICATION_H_
