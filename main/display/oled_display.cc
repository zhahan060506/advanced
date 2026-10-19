#include "oled_display.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"

#include <string>
#include <algorithm>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>

#include "images/image.h"
#define TAG "OledDisplay"
LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y, DisplayFonts fonts)
    : panel_io_(panel_io), panel_(panel), fonts_(fonts) {
    width_ = width;
    height_ = height;
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (dialogue_ != nullptr) {
        lv_obj_del(dialogue_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}
void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_label_set_text(chat_message_label_, content);
    lv_obj_clear_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::slide_to(OledDisplaySwitch new_index) {
    // 1. 获取新旧页面对象（确保指针有效，避免崩溃）
    lv_obj_t* old_page = pages[current_index_].page;
    lv_obj_t* new_page = pages[new_index].page;
    if (old_page == nullptr || new_page == nullptr) {
        ESP_LOGE("OledDisplay", "slide_to: old/new page is NULL!");
        return;
    }

    lv_obj_clear_flag(new_page, LV_OBJ_FLAG_HIDDEN);  // 显示新页面
    lv_obj_set_x(new_page, LV_HOR_RES);               // 强制放在右侧外（无论之前在哪里）
    lv_obj_set_y(new_page, 0);

    // 旧页面确保在屏幕内（防止之前的动画残留位置错误）
    lv_obj_set_x(old_page, 0);
    lv_obj_clear_flag(old_page, LV_OBJ_FLAG_HIDDEN);

    // 3. 定义两个独立的动画对象（避免复用导致动画覆盖）
    lv_anim_t anim_old;  // 旧页面动画（左滑出）
    lv_anim_t anim_new;  // 新页面动画（右滑入）
    const int ANIM_DURATION = 300;  // 动画时长（300ms，可调整）

    // --------------------------
    // 4. 配置旧页面动画（左滑出）
    // --------------------------
    lv_anim_init(&anim_old);
    lv_anim_set_var(&anim_old, old_page);  // 动画作用于旧页面
    // 起始位置：旧页面当前X（正常显示时为0）；结束位置：屏幕左侧外（-screen_width）
    lv_anim_set_values(&anim_old, lv_obj_get_x(old_page), -128);
    lv_anim_set_duration(&anim_old, ANIM_DURATION);  // 动画时长
    lv_anim_set_exec_cb(&anim_old, (lv_anim_exec_xcb_t)lv_obj_set_x);  // 改变X坐标
    lv_anim_set_path_cb(&anim_old, lv_anim_path_ease_out);  // 缓出效果（滑动更自然）
    // 动画结束后隐藏旧页面（避免在屏幕外占用资源）
    lv_anim_set_ready_cb(&anim_old, [](lv_anim_t* a) {
        lv_obj_t* page = (lv_obj_t*)a->var;
        lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(page, LV_HOR_RES);  // 关键：旧页面用完后移回右侧外
    });

    // --------------------------
    // 5. 配置新页面动画（右滑入）
    // --------------------------
    lv_obj_set_x(new_page, 128);             // 右侧外部（X=屏幕宽度）
    lv_obj_set_y(new_page, 0);                        // Y坐标保持顶部对齐
    lv_anim_init(&anim_new);
    lv_anim_set_var(&anim_new, new_page);  // 动画作用于新页面
    // 起始位置：屏幕右侧外（screen_width）；结束位置：屏幕内（0）
    lv_anim_set_values(&anim_new, lv_obj_get_x(new_page), 0);
    lv_anim_set_duration(&anim_new, ANIM_DURATION);  // 与旧页面动画时长一致，确保同步
    lv_anim_set_exec_cb(&anim_new, (lv_anim_exec_xcb_t)lv_obj_set_x);  // 改变X坐标
    lv_anim_set_path_cb(&anim_new, lv_anim_path_ease_out);  // 缓出效果，与旧页面统一

    lv_anim_start(&anim_old);
    lv_anim_start(&anim_new);

    // 更新当前索引
    current_index_ = new_index;
}

void OledDisplay::next(void) {
    DisplayLockGuard lock(this); 
    current_page_ = PAGE_SWITCH;
    OledDisplaySwitch new_index = (OledDisplaySwitch)((current_index_ + 1) % Switch_Count);
    slide_to(new_index);
}


void OledDisplay::Health_Check_Page_Init(lv_obj_t * screen) {
    health_screen_   = lv_obj_create(screen);
    lv_obj_align(health_screen_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_layout(health_screen_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(health_screen_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(health_screen_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(health_screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(health_screen_, fonts_.text_font, 0);

    lv_obj_t *heart = lv_obj_create(health_screen_);
    lv_obj_set_size(heart, 120, LV_SIZE_CONTENT);
    lv_obj_set_layout(heart, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(heart, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(heart, 0, LV_PART_MAIN);

    lv_obj_t *img1 = lv_img_create(heart);
    lv_img_set_src(img1, &heart_image);
    lv_obj_set_size(img1, 20, 20);
    lv_obj_t *text1 = lv_label_create(heart);
    lv_label_set_text(text1, "心率 :"); 
    heart_value_ = lv_label_create(heart);
    lv_label_set_text(heart_value_, "00.0");


    lv_obj_t *spo2 = lv_obj_create(health_screen_);
    lv_obj_set_size(spo2, 120, LV_SIZE_CONTENT);
    lv_obj_set_layout(spo2, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(spo2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(spo2, 0, LV_PART_MAIN);

    lv_obj_t *img2 = lv_img_create(spo2);
    lv_img_set_src(img2, &spo2_image);
    lv_obj_set_size(img2, 20, 20);
    lv_obj_t *text2 = lv_label_create(spo2);
    lv_label_set_text(text2, "血氧 :"); 
    spo2_value_ = lv_label_create(spo2);
    lv_label_set_text(spo2_value_, "00.0");
    
}

void OledDisplay::Main_Page_Init(lv_obj_t * screen) {
    main_screen_ = lv_obj_create(screen);
    lv_obj_set_style_text_font(main_screen_, fonts_.text_font, 0);
    lv_obj_set_style_text_color(main_screen_, lv_color_black(), 0);
    lv_obj_set_size(main_screen_, LV_HOR_RES, LV_VER_RES);  
    lv_obj_set_style_pad_all(main_screen_, 0, 0);           
    lv_obj_set_style_border_width(main_screen_, 0, 0);     
    lv_obj_add_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);

   
   
    // 主页面容器
    home_ = lv_obj_create(main_screen_);
    lv_obj_set_size(home_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(home_, 0, 0);
    lv_obj_set_style_border_width(home_, 0, 0);

    // 时间容器
    date_ = lv_obj_create(home_);
    lv_obj_set_size(date_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // 高度自适应内容
    lv_obj_set_style_pad_all(date_, 0, 0);
    lv_obj_set_style_border_width(date_, 0, 0);
    lv_obj_align(date_, LV_ALIGN_TOP_LEFT, 2, 2); // 左上角偏移10px（避免贴边）
    
    // 时间
    date_label_ = lv_label_create(date_);
    lv_obj_set_style_text_font(date_label_, fonts_.text_font, 0);  // 小号字体
    lv_label_set_text(date_label_, "00-00");  // 设置你的顶部文本
    lv_obj_align(date_label_, LV_ALIGN_LEFT_MID, 0, 0); 
    lv_obj_center(date_label_);
    // 1. 创建顶部右侧容器（用于包裹网络+电量，整体右对齐）
    lv_obj_t* top_right_container = lv_obj_create(main_screen_);
    lv_obj_set_size(top_right_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // 大小自适应内容
    lv_obj_set_style_pad_all(top_right_container, 0, 0);                     // 取消容器内边距
    lv_obj_set_style_border_width(top_right_container, 0, 0);                // 取消边框
    lv_obj_set_style_bg_opa(top_right_container, LV_OPA_TRANSP, 0);          // 透明背景（避免遮挡）
    lv_obj_set_flex_flow(top_right_container, LV_FLEX_FLOW_ROW);             // 横向排列子元素
    lv_obj_set_flex_align(top_right_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 子元素居中
    lv_obj_set_style_pad_gap(top_right_container, 2, 0);                    // 网络与电量的间距（5px，可调整）
    lv_obj_align(top_right_container, LV_ALIGN_TOP_RIGHT, -2, 2);           // 容器整体右对齐（右上角偏移-2,2）
    lv_obj_set_style_text_font(top_right_container, fonts_.icon_font, 0);
     // 2. 网络状态标签（放入容器中，不再单独对齐）
    page_network_label_ = lv_label_create(top_right_container);
    lv_label_set_text(page_network_label_, "");  // 示例：网络图标（替换为你的实际图标，如WiFi图标）
    
    // 3. 电量标签（放入容器中，在网络标签右侧）
    battery_label_ = lv_label_create(top_right_container);
    lv_label_set_text(battery_label_, "");  // 或具体电量图标：""（满电）、""（75%）等
    lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);

    // 无需单独align，容器会自动排列在网络标签右侧
    //天气容器
    weather_ = lv_obj_create(home_);
    lv_obj_set_flex_flow(weather_, LV_FLEX_FLOW_ROW); 
    lv_obj_set_flex_align(weather_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(weather_, 10, 0);  
    lv_obj_set_size(weather_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(weather_, 2, 0); 
    lv_obj_set_style_border_width(weather_, 0, 0);
    lv_obj_align(weather_, LV_ALIGN_BOTTOM_MID, 0, 0); 
    //城市
    city_text_ = lv_label_create(weather_);
    lv_label_set_text(city_text_, "未知");
    lv_obj_set_style_text_font(city_text_, fonts_.text_font, 0); 
    lv_obj_set_style_align(city_text_, LV_ALIGN_CENTER, 0);  

    //天气图标
    weather_icon_ = lv_img_create(weather_);

    //温度文本
    temp_text_ = lv_label_create(weather_);
    lv_label_set_text(temp_text_, "0°C");
    lv_obj_set_style_text_font(temp_text_, fonts_.text_font, 0); 
    lv_obj_set_style_align(temp_text_, LV_ALIGN_CENTER, 0);

    // 3. 中间区域 - 大号字体（核心内容）
    time_ = lv_obj_create(home_);
    lv_obj_set_size(time_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // 占据大部分空间
    lv_obj_set_style_pad_all(time_, 0, 0);
    lv_obj_set_style_border_width(time_, 0, 0);
    lv_obj_align(time_, LV_ALIGN_CENTER, 0, 0);

    page_time_label_ = lv_label_create(time_);
    lv_label_set_long_mode(page_time_label_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(page_time_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(page_time_label_, fonts_.text_large_font, 0);  // 常规字体
    lv_label_set_text(page_time_label_, "--:--");
    lv_obj_align(page_time_label_, LV_ALIGN_CENTER, 0, 0); 
    lv_obj_center(page_time_label_);  

}

void OledDisplay::Switch_Page_Init(lv_obj_t * screen) {
    int i = 0;
    lv_anim_init(&anim);
    lv_anim_set_duration(&anim, 500);                  // 动画持续500ms
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out); // 缓入缓出曲线
    
    for (i = 0; i < Switch_Count; i++) {
        // 创建页面容器
        pages[i].page = lv_obj_create(screen);
        lv_obj_set_size(pages[i].page, LV_HOR_RES, LV_VER_RES);  
        lv_obj_set_style_pad_all(pages[i].page, 0, 0);
        lv_obj_set_flex_flow(pages[i].page, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(pages[i].page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        // 创建图片
        pages[i].img_select = lv_img_create(pages[i].page);
        lv_img_set_src(pages[i].img_select, home_options[i].img);
        lv_obj_set_size(pages[i].img_select, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(pages[i].img_select, 10, 0);
        lv_obj_set_style_shadow_width(pages[i].img_select, 5, 0);
        // 创建文字标签
        pages[i].label_select = lv_label_create(pages[i].page);
        lv_label_set_text(pages[i].label_select, home_options[i].text);
        lv_obj_set_style_text_font(pages[i].label_select, fonts_.text_font, 0);  // 小号字体
        lv_obj_set_style_align(pages[i].label_select, LV_ALIGN_CENTER, 0);
        lv_obj_set_style_margin_top(pages[i].label_select, 0, 0);
        if (i == 0) {
            lv_obj_set_x(pages[i].page, 0);
            lv_obj_clear_flag(pages[i].page, LV_OBJ_FLAG_HIDDEN);       // 可见
        } else {
            lv_obj_set_x(pages[i].page, LV_HOR_RES);                    // 右侧外部，看不到
            lv_obj_add_flag(pages[i].page, LV_OBJ_FLAG_HIDDEN);         // 隐藏，避免占用绘制资源
        }
    }
}

void OledDisplay::Dialougue_Page_Init(lv_obj_t * screen) {
    dialogue_screen_ = lv_obj_create(screen);
    lv_obj_set_style_text_font(dialogue_screen_, fonts_.text_font, 0);
    lv_obj_set_style_text_color(dialogue_screen_, lv_color_black(), 0);
    lv_obj_add_flag(dialogue_screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(dialogue_screen_, 0, 0);  // 强制定位到屏幕左上角
    lv_obj_set_size(dialogue_screen_, LV_HOR_RES, LV_VER_RES);  // 强制全屏尺寸
    lv_obj_set_style_pad_all(dialogue_screen_, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(dialogue_screen_, 0, LV_PART_MAIN);
    /* Container */
    container_ = lv_obj_create(dialogue_screen_);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    // 关键：设置垂直对齐为顶部对齐，子元素从顶部开始排列
    lv_obj_set_flex_align(container_, 
        LV_FLEX_ALIGN_START,  // 主轴（垂直）顶部对齐
        LV_FLEX_ALIGN_CENTER, // 交叉轴（水平）居中
        LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0); // 状态栏与内容区间距为0

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES); 
    lv_obj_set_height(content_, LV_VER_RES - 16); 
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(content_, 8, 0); 

    // 创建左侧固定宽度的容器
    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);  // 固定宽度32像素
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);

    // 创建右侧可扩展的容器
    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // 延迟一定的时间后开始滚动字幕
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(30, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
        lv_obj_add_flag(mute_label_, LV_OBJ_FLAG_HIDDEN);

    // battery_label_ = lv_label_create(status_bar_);
    // lv_label_set_text(battery_label_, "");
    // lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    //     lv_obj_add_flag(battery_label_, LV_OBJ_FLAG_HIDDEN);
    low_battery_popup_ = lv_obj_create(dialogue_screen_);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::Wait_Page_Init(lv_obj_t * screen) {


}

void OledDisplay::SetupUI_128x64() {
    int i = 0;
    DisplayLockGuard lock(this);
    default_screen = lv_screen_active();

    Switch_Page_Init(default_screen);
    Main_Page_Init(default_screen);
    Dialougue_Page_Init(default_screen);
    Health_Check_Page_Init(default_screen);
    Wait_Page_Init(default_screen);
   
}

void OledDisplay::SetupUI_128x32() {
   
}

