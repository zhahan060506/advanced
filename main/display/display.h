#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include <chrono>
#define Switch_Count 3

LV_IMG_DECLARE(zhuye)
LV_IMG_DECLARE(duihua)
LV_IMG_DECLARE(jiance)
LV_IMG_DECLARE(spo2_image)
LV_IMG_DECLARE(heart_image)

struct Select_Page{
    lv_obj_t *img_select;
    lv_obj_t *label_select;
    lv_obj_t *page;
};

struct DisplayPage {
    const lv_img_dsc_t* img;
    const char* text;
};
enum OledDisplayPage {
    PAGE_SWITCH = 0,    // 选择页面
    PAGE_MAIN,          // 主页面
    PAGE_CHAT,          // 对话页面
    PAGE_DETE,          //  健康检测页面
    PAGE_INIT,
    PAGE_TIMER,
};

enum OledDisplaySwitch {
    Switch_MAIN = 0,      // 选择主页面
    Switch_CHAT,          // 选择对话页面
    Switch_DETE,          // 选择健康检测页面
};
struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* text_large_font = nullptr;    // 大号文本字体
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

class Display {
public:

    Display();
    virtual ~Display();
    void UpdateTime(void);
    void UpdateDate(void);
    void UpdateWeather(void);
    void UpDateNetWork(void);
    void UpDateBattery(void);
    void UpDateHealth(void);
    void Switch_Main_Page(void);
    void Switch_Dialogue_Page(void);
    void Switch_Timer_Page(void);
    void Switch_Health_Check_Page(void);
    void SetTimerRemaining(int seconds);
    void SetTimerText(const char* text);

    void Return_Switch_Page(void);
    virtual void SetTime(const char* time_str);
    virtual void SetDate(const char* date_str);
    virtual void SetWeather(void);

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetIcon(const char* icon);
    virtual void SetPreviewImage(const lv_img_dsc_t* image);
    virtual void SetTheme(const std::string& theme_name);
    virtual std::string GetTheme() { return current_theme_name_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void ShowStandbyScreen(bool show);
    
    virtual void next(void);

    OledDisplayPage GetCurrentPage() const;  // 关键：添加此声明
    OledDisplaySwitch GetCurrentIndex() const;  // 关键：添加此声明
    inline int width() const { return width_; }
    inline int height() const { return height_; }


protected:
    const char *week_days[7] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    int width_ = 0;
    int height_ = 0;
    
    Select_Page pages[3] = {};
    OledDisplayPage current_page_ = PAGE_INIT;  // 当前页面
    OledDisplaySwitch current_index_ = Switch_MAIN;

    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    lv_obj_t *spo2_value_;
    lv_obj_t *heart_value_;
    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    lv_obj_t *weather_icon_ = nullptr;
    lv_obj_t *temp_text_ = nullptr;
    lv_obj_t *city_text_ = nullptr;

    lv_obj_t *temperature_label_ = nullptr;


    lv_obj_t* weather_container_ = nullptr;
    // lv_obj_t* init_screen_ = nullptr;
    // lv_obj_t* init_notification_label_ = nullptr;
    // lv_obj_t* init_status_label_ = nullptr;
    // lv_obj_t* init_mute_label_ = nullptr;
    // lv_obj_t *init_icon_label_ = nullptr;

    lv_obj_t* main_screen_ = nullptr;
    lv_obj_t* dialogue_screen_ = nullptr;
    lv_obj_t* timer_screen_ = nullptr;
    lv_obj_t* health_screen_ = nullptr;

    lv_obj_t* container_ = nullptr;
    lv_obj_t* init_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    lv_obj_t* home_ = nullptr;
    lv_obj_t* dialogue_ = nullptr;
    lv_obj_t* page_time_label_ = nullptr;
    lv_obj_t* time_label_ = nullptr;
    lv_obj_t* time_ = nullptr;

    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* date_ = nullptr;

    lv_obj_t* weather_label_ = nullptr;
    lv_obj_t* weather_ = nullptr;
    lv_obj_t* timer_value_label_ = nullptr;

    lv_obj_t* page_network_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    std::chrono::system_clock::time_point last_status_update_time_;
    esp_timer_handle_t notification_timer_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

     DisplayFonts fonts_;  // 字体配置
};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
