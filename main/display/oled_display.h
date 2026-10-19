#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>



class OledDisplay : public Display {
private:
   
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_obj_t* default_screen;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    struct DisplayPage home_options[3] = {
        {&zhuye, "主页"}, 
        {&duihua, "对话"}, 
        {&jiance, "健康检测"}};
   
    // 动画效果
    lv_anim_t anim;
    static const int ANIM_DURATION = 300;
    DisplayFonts fonts_;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
    
    void Switch_Page_Init(lv_obj_t * screen);
    void Main_Page_Init(lv_obj_t * screen);
    void Health_Check_Page_Init(lv_obj_t * screen);
    void Dialougue_Page_Init(lv_obj_t * screen);
    void Wait_Page_Init(lv_obj_t *screen);
    
    virtual void next(void) override;
    
    void SetupUI_128x64();
    void SetupUI_128x32();
    void slide_to(OledDisplaySwitch new_index);
public:
    
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y,
                DisplayFonts fonts);
    ~OledDisplay();

    virtual void SetChatMessage(const char* role, const char* content) override;
};

#endif // OLED_DISPLAY_H
