#include "power.h"
#include "esp_log.h"
#define TAG "POWER"
BatteryMonitor::BatteryMonitor(){
  
    adc_handle = nullptr;
    voltage_div_ratio = (float)( (R1_VALUE + R2_VALUE) / R2_VALUE);  // 双10K分压比=2.0
    adc_init();
    adc_calibration_init();
}

BatteryMonitor::~BatteryMonitor() {
    if (is_calibrated && adc_cali_handle) {
        adc_cali_delete_scheme_curve_fitting(adc_cali_handle);
    }
    // 释放ADC资源
    if (adc_handle) {
        adc_oneshot_del_unit(adc_handle);
    }
}

void BatteryMonitor::adc_init() {
    // 配置ADC2单元
    adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_cfg, &adc_handle));

    // 配置ADC2-CH7通道
    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .atten = ADC_ATTEN_DB_12,  // 11dB衰减支持0-3.3V采样
        .bitwidth = ADC_BITWIDTH_12,  // 12位精度
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &adc_chan_cfg));
}

// 初始化新ADC校准（消除"legacy校准"警告）
void BatteryMonitor::adc_calibration_init() {
    // 初始化校准方案
    
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BIT_WIDTH,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    is_calibrated = ret == ESP_OK;
    if (!is_calibrated) ESP_LOGW(TAG, "ADC calibration unavailable; battery reading disabled");
}

uint32_t BatteryMonitor::read_adc_voltage() {
    if (!is_calibrated || !adc_cali_handle) return 0;
    // 多次采样取平均
    uint32_t total_raw = 0;
    const uint8_t samples = 5;
    uint8_t valid_samples = 0;

    for (uint8_t i = 0; i < samples; i++) {
        int raw;        
        esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
        if (ret == ESP_OK && raw >= 0) {
            total_raw += raw;
            valid_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // 计算平均原始值
    if (valid_samples == 0) return 0;
    float avg_raw = (float)(total_raw) / valid_samples;
    int voltage = 0;
    if (adc_cali_raw_to_voltage(adc_cali_handle, avg_raw, &voltage) != ESP_OK || voltage < 0) return 0;
    printf("实际的电压：%d， adc采集的值：%.1f\n", voltage, avg_raw);
    // 使用基础校准公式（替代line fitting方案）
    // 参考电压1100mV，12位精度(0-4095),  × 衰减倍数
    //return (uint32_t)((avg_raw * 1100.0f) / 4095.0f * 3.3f);
    return voltage;
}

uint32_t BatteryMonitor::get_battery_voltage() {
    uint32_t adc_voltage = read_adc_voltage();
    if (adc_voltage == 0) {
        return 0;
    }
    return (uint32_t)(adc_voltage * voltage_div_ratio);
}

uint8_t BatteryMonitor::get_battery_percent() {
    uint32_t voltage = get_battery_voltage();
    if (voltage == 0) {
        return 0;
    }
    printf("电压：%ld\n", voltage);
    // 电压到百分比转换（3.0V=0%，4.2V=100%）
    if (voltage <= 3000) {
        return 0;
    } else if (voltage >= 4200) {
        return 100;
    } else {
        // 线性计算
        return (uint8_t)(((voltage - 3000) * 100) / 1200);
    }
}

uint32_t BatteryMonitor::estimate_remaining_time(uint16_t current_ma) {
    if (current_ma == 0) return 0;

    uint8_t percent = get_battery_percent();
    uint32_t remaining_capacity = (BATTERY_CAPACITY_MAH * percent) / 100;
    return (remaining_capacity / current_ma) * 60;  // 转换为分钟
}
