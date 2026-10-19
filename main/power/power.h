#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "esp_adc/adc_oneshot.h"       
#include "esp_adc/adc_cali.h"          
#include "esp_adc/adc_cali_scheme.h"  
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 硬件配置（双10K电阻分压）
#define R1_VALUE               20000    // 上拉电阻：20KΩ
#define R2_VALUE               10000    // 下拉电阻：10KΩ
#define ADC_CHANNEL            ADC_CHANNEL_7   // ADC1-CH7（GPIO8）
#define ADC_ATTEN              ADC_ATTEN_DB_12 // 11dB衰减（支持3.9V采样）
#define ADC_BIT_WIDTH          ADC_BITWIDTH_12 // 12位精度
#define ADC_UNIT               ADC_UNIT_1
// 电池参数
#define BATTERY_FULL_VOLTAGE   4200     // 满电电压：4.2V（mV）
#define BATTERY_EMPTY_VOLTAGE  3000     // 低电电压：3.0V（mV）
#define BATTERY_CAPACITY_MAH   150      // 电池容量：300mAh（根据实际修改）

class BatteryMonitor {
private:
    adc_oneshot_unit_handle_t adc_handle;       // ADC单拍模式句柄（v5.3新特性）
    adc_cali_handle_t adc_cali_handle = NULL;
    bool is_calibrated;                         // 校准状态
    float voltage_div_ratio;                    // 分压比：(R1+R2)/R2 = 2
    void adc_init();
    uint32_t read_adc_voltage() ;
    void adc_calibration_init();

public:
    BatteryMonitor();
    ~BatteryMonitor();
    uint32_t get_battery_voltage();              // 获取电池电压（mV）
    uint8_t get_battery_percent();              // 获取电池百分比

    // 估算剩余续航时间（分钟）
    uint32_t estimate_remaining_time(uint16_t current_ma);
};

#endif // BATTERY_MONITOR_H
