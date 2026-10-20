#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "display.h"
#include "board.h"
#include "application.h"
#include "font_awesome_symbols.h"
#include "audio_codec.h"
#include "settings.h"
#include "assets/lang_config.h"
#include "weather_icon.cc"
LV_IMG_DECLARE(weather_icon)
#define TAG "Display"

#ifndef HEALTH_SENSOR_ENABLED
#define HEALTH_SENSOR_ENABLED 1
#endif

Display::Display() {
    // Notification timer
    esp_timer_create_args_t notification_timer_args = {
        .callback = [](void *arg) {
            Display *display = static_cast<Display*>(arg);
            DisplayLockGuard lock(display);
            lv_obj_add_flag(display->notification_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(display->status_label_, LV_OBJ_FLAG_HIDDEN);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "notification_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));

    // Create a power management lock
    auto ret = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "display_update", &pm_lock_);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "Power management not supported");
    } else {
        ESP_ERROR_CHECK(ret);
    }
}

Display::~Display() {
    if (gesture_volume_timer_) lv_timer_delete(gesture_volume_timer_);
    if (gesture_volume_) lv_obj_delete(gesture_volume_);
    if (notification_timer_ != nullptr) {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }

    if (network_label_ != nullptr) {
        lv_obj_del(network_label_);
        lv_obj_del(notification_label_);
        lv_obj_del(status_label_);
        lv_obj_del(mute_label_);
        lv_obj_del(battery_label_);
        lv_obj_del(emotion_label_);
    }
    if( low_battery_popup_ != nullptr ) {
        lv_obj_del(low_battery_popup_);
    }
    if (pm_lock_ != nullptr) {
        esp_pm_lock_delete(pm_lock_);
    }
}

// 实现获取当前页面的函数
OledDisplayPage Display::GetCurrentPage() const {
    return current_page_;  // 返回存储的当前页面
}

OledDisplaySwitch Display::GetCurrentIndex() const {
    return current_index_;  // 返回存储页面的索引
}

void Display::SetTime(const char* time_str)
{
    DisplayLockGuard lock(this);
    if (page_time_label_ == nullptr) {
        return;
    }
    if (strcmp(lv_label_get_text(page_time_label_), time_str) != 0) 
    {
        lv_label_set_text(page_time_label_, time_str);
    }
}

void Display::next(void)
{
   
}
void Display::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    if (status_label_ == nullptr) {
        return;
    }
    lv_label_set_text(status_label_, status);
    lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    last_status_update_time_ = std::chrono::system_clock::now();
}

void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    DisplayLockGuard lock(this);
    if (notification_label_ == nullptr) {
        return;
    }
    lv_label_set_text(notification_label_, notification);
    lv_obj_clear_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);

    esp_timer_stop(notification_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));
}
void Display::Switch_Dialogue_Page(void)
{
    DisplayLockGuard lock(this);
    if (info_panel_) lv_obj_add_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Switching to dialogue page");
    if (main_screen_ != nullptr) {
        lv_obj_add_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (timer_screen_ != nullptr) {
        lv_obj_add_flag(timer_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (current_page_!= PAGE_INIT)
    {
        current_page_ = PAGE_CHAT;
        if (pages[current_index_].page != nullptr) {
            lv_obj_add_flag(pages[current_index_].page, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (dialogue_screen_ == nullptr) {
        return;
    }
    lv_obj_clear_flag(dialogue_screen_, LV_OBJ_FLAG_HIDDEN);  // 显示对话页面
    current_page_ = PAGE_CHAT;
}

void Display::Return_Switch_Page(void)
{
    DisplayLockGuard lock(this);
    if (info_panel_) lv_obj_add_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Switching to switch page");
    if (current_page_== PAGE_MAIN) {
        if (main_screen_ != nullptr) {
            lv_obj_add_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else if ((current_page_== PAGE_CHAT) || (current_page_== PAGE_INIT))
    {
        if (dialogue_screen_ != nullptr) {
            lv_obj_add_flag(dialogue_screen_, LV_OBJ_FLAG_HIDDEN);
        }
        Application::GetInstance().SetDeviceState(kDeviceStateStop);
    }
    else if (current_page_== PAGE_DETE && health_screen_ != nullptr) 
            lv_obj_add_flag(health_screen_, LV_OBJ_FLAG_HIDDEN);
    Application::GetInstance().SetDeviceState(kDeviceStateSwitch);
    if (pages[current_index_].page != nullptr) {
        lv_obj_clear_flag(pages[current_index_].page, LV_OBJ_FLAG_HIDDEN);
    }
    current_page_ = PAGE_SWITCH;
}

void Display::Switch_Main_Page(void)
{
    DisplayLockGuard lock(this);
    if (info_panel_) lv_obj_add_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Switching to main page");
    if (dialogue_screen_ != nullptr) {
        lv_obj_add_flag(dialogue_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (timer_screen_ != nullptr) {
        lv_obj_add_flag(timer_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (pages[current_index_].page != nullptr) {
        lv_obj_add_flag(pages[current_index_].page, LV_OBJ_FLAG_HIDDEN);
    }
    if (main_screen_ == nullptr) {
        current_page_ = PAGE_MAIN;
        return;
    }
    lv_obj_clear_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);  // 显示主页面
    current_page_ = PAGE_MAIN;
    current_page_ = PAGE_MAIN;
}

void Display::Switch_Timer_Page(void)
{
    DisplayLockGuard lock(this);
    if (info_panel_) lv_obj_add_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Switching to timer page");
    if (main_screen_ != nullptr) {
        lv_obj_add_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (dialogue_screen_ != nullptr) {
        lv_obj_add_flag(dialogue_screen_, LV_OBJ_FLAG_HIDDEN);
    }
    if (pages[current_index_].page != nullptr) {
        lv_obj_add_flag(pages[current_index_].page, LV_OBJ_FLAG_HIDDEN);
    }
    if (timer_screen_ == nullptr) {
        return;
    }
    lv_obj_clear_flag(timer_screen_, LV_OBJ_FLAG_HIDDEN);
    current_page_ = PAGE_TIMER;
}

void Display::SetTimerRemaining(int seconds)
{
    DisplayLockGuard lock(this);
    if (timer_value_label_ == nullptr) {
        return;
    }
    lv_label_set_text_fmt(timer_value_label_, "%ds", seconds);
}

void Display::SetTimerText(const char* text)
{
    DisplayLockGuard lock(this);
    if (timer_value_label_ == nullptr) {
        return;
    }
    lv_label_set_text(timer_value_label_, text);
}

void Display::Switch_Info_Page(bool alarm, const std::string& text) {
    DisplayLockGuard lock(this);
    if (!main_screen_) return;
    if (!info_panel_) {
        info_panel_ = lv_obj_create(lv_obj_get_parent(main_screen_));
        lv_obj_set_size(info_panel_, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(info_panel_, lv_color_hex(0x101820), 0);
        lv_obj_set_style_bg_opa(info_panel_, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(info_panel_, lv_color_white(), 0);
        lv_obj_set_style_text_font(info_panel_, lv_obj_get_style_text_font(city_text_ ? city_text_ : main_screen_, LV_PART_MAIN), 0);
        lv_obj_set_style_border_width(info_panel_, 0, 0);
        lv_obj_set_style_radius(info_panel_, 0, 0);
        lv_obj_clear_flag(info_panel_, LV_OBJ_FLAG_SCROLLABLE);
        info_title_ = lv_label_create(info_panel_);
        lv_obj_align(info_title_, LV_ALIGN_TOP_MID, 0, 4);
        info_body_ = lv_label_create(info_panel_);
        lv_obj_set_width(info_body_, lv_pct(100));
        lv_obj_set_style_text_align(info_body_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(info_body_, LV_ALIGN_CENTER, 0, 0);
        auto* hint = lv_label_create(info_panel_);
        lv_label_set_text(hint, "后拉返回主页");
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }
    lv_label_set_text(info_title_, alarm ? "闹钟" : "天气");
    if (alarm) {
        lv_label_set_text(info_body_, text.c_str());
    } else {
        lv_label_set_text_fmt(info_body_, "%s\n%s",
            city_text_ ? lv_label_get_text(city_text_) : "--",
            temp_text_ ? lv_label_get_text(temp_text_) : "--");
    }
    lv_obj_move_foreground(info_panel_);
    lv_obj_clear_flag(info_panel_, LV_OBJ_FLAG_HIDDEN);
    current_page_ = alarm ? PAGE_ALARM : PAGE_WEATHER;
    ESP_LOGI(TAG, "Switching to %s page", alarm ? "alarm" : "weather");
}

void Display::UpdateAlarmInfo(const std::string& text) {
    DisplayLockGuard lock(this);
    if (current_page_ == PAGE_ALARM && info_body_) lv_label_set_text(info_body_, text.c_str());
}

void Display::ShowGestureVolume(int volume) {
    DisplayLockGuard lock(this);
    if (!main_screen_) return;
    if (!gesture_volume_) {
        gesture_volume_ = lv_label_create(lv_layer_top());
        lv_obj_set_style_text_font(gesture_volume_, lv_obj_get_style_text_font(city_text_ ? city_text_ : main_screen_, LV_PART_MAIN), 0);
        lv_obj_set_style_bg_color(gesture_volume_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(gesture_volume_, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(gesture_volume_, lv_color_white(), 0);
        lv_obj_set_style_pad_all(gesture_volume_, 8, 0);
        gesture_volume_timer_ = lv_timer_create([](lv_timer_t* timer) {
            lv_obj_add_flag(static_cast<lv_obj_t*>(lv_timer_get_user_data(timer)), LV_OBJ_FLAG_HIDDEN);
            lv_timer_pause(timer);
        }, 1500, gesture_volume_);
    }
    lv_label_set_text_fmt(gesture_volume_, "音量 %d%%", volume);
    lv_obj_align(gesture_volume_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_clear_flag(gesture_volume_, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(gesture_volume_timer_);
    lv_timer_resume(gesture_volume_timer_);
}

void Display::Switch_Health_Check_Page(void)
{
#if !HEALTH_SENSOR_ENABLED
    ESP_LOGW(TAG, "Health sensor is disabled for this board");
    return;
#else
    
    auto max30102 = Board::GetInstance().GetMAX30102();
    ESP_LOGI(TAG, "Switching to Health_Check page");
    DisplayLockGuard lock(this);
    lv_label_set_text_fmt(heart_value_, "%d", max30102->Max30102_Get_Heart());
    lv_label_set_text_fmt(spo2_value_, "%d", max30102->Max30102_Get_SpO2());
    lv_obj_add_flag(pages[current_index_].page, LV_OBJ_FLAG_HIDDEN);  
    lv_obj_clear_flag(health_screen_, LV_OBJ_FLAG_HIDDEN);  // 显示健康检测页面
    current_page_ = PAGE_DETE;
    max30102->max30102_config();
    Application::GetInstance().SetDeviceState(kDeviceStateHealth);
#endif
}
void Display::UpdateTime(void)
{
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    // Check if the we have already set the time
    if (tm->tm_year >= 2025 - 1900) {
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%H:%M ", tm);
        SetTime(time_str);
    } else {
        ESP_LOGW(TAG, "System time is not set, tm_year: %d", tm->tm_year);
    }
}

void Display::SetDate(const char* date_str)
{
    DisplayLockGuard lock(this);
    if (date_label_ == nullptr) {
        return;
    }
    lv_label_set_text(date_label_, date_str);
}


void Display::UpdateDate(void)
{
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    if (tm->tm_year >= 2025 - 1900) {
        char date_str[32];
        strftime(date_str, sizeof(date_str), "%m-%d ", tm);
        sprintf(date_str + strlen(date_str), "%s", week_days[tm->tm_wday]);
        SetDate(date_str);
    } else {
        ESP_LOGW(TAG, "System time is not set, tm_year: %d", tm->tm_year);
    }
}

void Display::SetWeather(void)
{
    DisplayLockGuard lock(this);
    auto& board = Board::GetInstance();
    auto weather = board.GetWeather();
    char weather_data[128] = {0};
    memset(weather_data, 0, sizeof(weather_data));
    snprintf(weather_data, sizeof(weather_data)-1, "%s %s",
                        weather->Get_City_Value(),
                        weather->Get_Weather_Value());
    if (city_text_ == nullptr)
        return ;
    lv_label_set_text(city_text_, weather_data);

    memset(weather_data, 0, sizeof(weather_data));
    snprintf(weather_data, sizeof(weather_data)-1, "%s℃",
                        weather->Get_Temperature_Value());
    if (temp_text_ == nullptr)
        return;
    lv_label_set_text(temp_text_, weather_data);
    if (current_page_ == PAGE_WEATHER && info_body_) {
        lv_label_set_text_fmt(info_body_, "%s\n%s", lv_label_get_text(city_text_), weather_data);
    }

    if (weather_icon_ == nullptr)
        return;
    uint8_t code = weather->Get_Code();
    switch (code)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            lv_img_set_src(weather_icon_, &qingtian);           /* 晴天 */
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            lv_img_set_src(weather_icon_, &duoyun);             /* 多云 */
            break;
        case 9:
            lv_img_set_src(weather_icon_, &yintian);            /* 阴天 */
            break;
        case 10:
        case 11:
        case 12:
            lv_img_set_src(weather_icon_, &leizhenyu);           /* 阵雨 */
            break;
        case 13:
            lv_img_set_src(weather_icon_, &xiaoyu);              /* 小雨 */
            break;
        case 14:
            lv_img_set_src(weather_icon_, &zhongyu);             /* 中雨 */
            break;
        case 15:
            lv_img_set_src(weather_icon_, &dayu);                /* 大雨 */
            break;
        case 16:
        case 17:
        case 18:
            lv_img_set_src(weather_icon_, &dabaoyu);                /* 暴雨 */
            break;
        case 19:
        case 20:
            lv_img_set_src(weather_icon_, &yujiaxue);               /* 雨夹雪 */
            break;
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            lv_img_set_src(weather_icon_, &xue);                    /* 下雪 */
            break;
        case 30:
            lv_img_set_src(weather_icon_, &wu);                    /* 雾 */
            break;
        case 31:
            lv_img_set_src(weather_icon_, &mai);                    /* 霾 */
            break;
        default:
            break;
    }
        
}

void Display::UpdateWeather(void)
{
    if (weather_update_running_.exchange(true)) return;
    // HTTP/TLS must never block the shared ESP timer task or the voice loop.
    if (xTaskCreate([](void* arg) {
        auto* display = static_cast<Display*>(arg);
        auto* weather = Board::GetInstance().GetWeather();
        if (weather && weather->Get_Weather()) display->SetWeather();
        display->weather_update_running_.store(false);
        vTaskDelete(nullptr);
    }, "weather_update", 8192, this, 2, nullptr) != pdPASS) {
        weather_update_running_.store(false);
        ESP_LOGW(TAG, "Cannot create weather task; keeping last weather");
    }
}

void Display::UpDateNetWork(void)
{
    DisplayLockGuard lock(this);

    const char* icon = nullptr;
    auto& board = Board::GetInstance();
    icon = board.GetNetworkStateIcon();
    if (page_network_label_ != nullptr && icon != nullptr && network_icon_ != icon) {
        DisplayLockGuard lock(this);
        network_icon_ = icon;
        lv_label_set_text(page_network_label_, network_icon_);
    }
}
void Display::UpDateHealth(void)
{
#if !HEALTH_SENSOR_ENABLED
    return;
#else
    DisplayLockGuard lock(this);
    if ((heart_value_ == NULL) || (spo2_value_ == NULL))
        return;
    auto max30102 = Board::GetInstance().GetMAX30102();
    lv_label_set_text_fmt(heart_value_, "%d", max30102->Max30102_Get_Heart());
    lv_label_set_text_fmt(spo2_value_, "%d", max30102->Max30102_Get_SpO2());
    ESP_LOGI(TAG, "Heart: %d, SpO2: %d", max30102->Max30102_Get_Heart(), max30102->Max30102_Get_SpO2());
#endif
}


void Display::UpDateBattery(void)
{
    if (battery_label_ == nullptr) return;
    if (pm_lock_) esp_pm_lock_acquire(pm_lock_);
    const char* icon = nullptr;
    auto Battery = Board::GetInstance().GetBatteryMonitor();
    int battery_level = std::clamp<int>(Battery->get_battery_percent(), 0, 100);
    const char* levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                FONT_AWESOME_BATTERY_1,    // 20-39%
                FONT_AWESOME_BATTERY_2,    // 40-59%
                FONT_AWESOME_BATTERY_3,    // 60-79%
                FONT_AWESOME_BATTERY_FULL, // 80-99%
                FONT_AWESOME_BATTERY_FULL, // 100%
            };
    icon = levels[battery_level / 20];
    
    // printf("Percent: %d%%\n",battery_level);
    DisplayLockGuard lock(this);
    lv_label_set_text(battery_label_, icon);  // 或具体电量图标：""（满电）、""（75%）等
    // lv_label_set_text_fmt(battery_label_, "%d%%", battery_level);
    if (pm_lock_) esp_pm_lock_release(pm_lock_);
}


void Display::UpdateStatusBar(bool update_all) {
    // auto& app = Application::GetInstance();
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    // const char* icon = nullptr;


    // // 每 10 秒更新一次网络图标
    // static int seconds_counter = 0;
    // if (update_all || seconds_counter++ % 10 == 0) {
    //     // 升级固件时，不读取 4G 网络状态，避免占用 UART 资源
    //     auto device_state = Application::GetInstance().GetDeviceState();
    //     static const std::vector<DeviceState> allowed_states = {
    //         kDeviceStateIdle,
    //         kDeviceStateStarting,
    //         kDeviceStateWifiConfiguring,
    //         kDeviceStateListening,
    //         kDeviceStateActivating,
    //     };
    //     if (std::find(allowed_states.begin(), allowed_states.end(), device_state) != allowed_states.end()) {
    //        UpDateNetWork();
    //     }
    // }

    
    // Update mute icon
    {
        DisplayLockGuard lock(this);
        if (mute_label_ == nullptr) {
            return;
        }
        // 如果静音状态改变，则更新图标
        if (codec->output_volume() == 0 && !muted_) {
            muted_ = true;
            lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_MUTE);
        } else if (codec->output_volume() > 0 && muted_) {
            muted_ = false;
            lv_label_set_text(mute_label_, "");
        }
    }

    // Update time
    if (current_page_ == PAGE_CHAT) {
        if (last_status_update_time_ + std::chrono::seconds(10) < std::chrono::system_clock::now()) {
            // Set status to clock "HH:MM"
            time_t now = time(NULL);
            struct tm* tm = localtime(&now);
            // Check if the we have already set the time
            if (tm->tm_year >= 2025 - 1900) {
                char time_str[16];
                strftime(time_str, sizeof(time_str), "%H:%M  ", tm);
                SetStatus(time_str);
            } else {
                ESP_LOGW(TAG, "System time is not set, tm_year: %d", tm->tm_year);
            }
        }
    }
    if (current_page_ != PAGE_CHAT) 
        return; 
    // // 更新电池图标
    // int battery_level;
    // bool charging, discharging;
    // if (board.GetBatteryLevel(battery_level, charging, discharging)) {
    //     if (charging) {
    //         icon = FONT_AWESOME_BATTERY_CHARGING;
    //     } else {
    //         const char* levels[] = {
    //             FONT_AWESOME_BATTERY_EMPTY, // 0-19%
    //             FONT_AWESOME_BATTERY_1,    // 20-39%
    //             FONT_AWESOME_BATTERY_2,    // 40-59%
    //             FONT_AWESOME_BATTERY_3,    // 60-79%
    //             FONT_AWESOME_BATTERY_FULL, // 80-99%
    //             FONT_AWESOME_BATTERY_FULL, // 100%
    //         };
    //         icon = levels[battery_level / 20];
    //     }
    //     DisplayLockGuard lock(this);
    //     if (battery_label_ != nullptr && battery_icon_ != icon) {
    //         battery_icon_ = icon;
    //         lv_label_set_text(battery_label_, battery_icon_);
    //     }

    //     if (low_battery_popup_ != nullptr) {
    //         if (strcmp(icon, FONT_AWESOME_BATTERY_EMPTY) == 0 && discharging) {
    //             if (lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) { // 如果低电量提示框隐藏，则显示
    //                 lv_obj_clear_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    //                 app.PlaySound(Lang::Sounds::P3_LOW_BATTERY);
    //             }
    //         } else {
    //             // Hide the low battery popup when the battery is not empty
    //             if (!lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) { // 如果低电量提示框显示，则隐藏
    //                 lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    //             }
    //         }
        // }
    // }
    // esp_pm_lock_release(pm_lock_);
}


void Display::SetEmotion(const char* emotion) {
    struct Emotion {
        const char* icon;
        const char* text;
    };

    static const std::vector<Emotion> emotions = {
        {FONT_AWESOME_EMOJI_NEUTRAL, "neutral"},
        {FONT_AWESOME_EMOJI_HAPPY, "happy"},
        {FONT_AWESOME_EMOJI_LAUGHING, "laughing"},
        {FONT_AWESOME_EMOJI_FUNNY, "funny"},
        {FONT_AWESOME_EMOJI_SAD, "sad"},
        {FONT_AWESOME_EMOJI_ANGRY, "angry"},
        {FONT_AWESOME_EMOJI_CRYING, "crying"},
        {FONT_AWESOME_EMOJI_LOVING, "loving"},
        {FONT_AWESOME_EMOJI_EMBARRASSED, "embarrassed"},
        {FONT_AWESOME_EMOJI_SURPRISED, "surprised"},
        {FONT_AWESOME_EMOJI_SHOCKED, "shocked"},
        {FONT_AWESOME_EMOJI_THINKING, "thinking"},
        {FONT_AWESOME_EMOJI_WINKING, "winking"},
        {FONT_AWESOME_EMOJI_COOL, "cool"},
        {FONT_AWESOME_EMOJI_RELAXED, "relaxed"},
        {FONT_AWESOME_EMOJI_DELICIOUS, "delicious"},
        {FONT_AWESOME_EMOJI_KISSY, "kissy"},
        {FONT_AWESOME_EMOJI_CONFIDENT, "confident"},
        {FONT_AWESOME_EMOJI_SLEEPY, "sleepy"},
        {FONT_AWESOME_EMOJI_SILLY, "silly"},
        {FONT_AWESOME_EMOJI_CONFUSED, "confused"}
    };
    
    // 查找匹配的表情
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });
    
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, FONT_AWESOME_EMOJI_NEUTRAL);
    }
}

void Display::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_label_set_text(emotion_label_, icon);
}

void Display::SetPreviewImage(const lv_img_dsc_t* image) {
    // Do nothing
}

void Display::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_label_set_text(chat_message_label_, content);
}

void Display::SetTheme(const std::string& theme_name) {
    current_theme_name_ = theme_name;
    Settings settings("display", true);
    settings.SetString("theme", theme_name);
}

void Display::ShowStandbyScreen(bool show) {
    if (show) {
        SetChatMessage("system", "");
        SetEmotion("sleepy");
    } else {
        SetChatMessage("system", "");
        SetEmotion("neutral");
    }
}
