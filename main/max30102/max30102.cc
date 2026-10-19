#include "max30102.h"
#include "esp_log.h"
#include "esp_check.h"
#include "string.h"
#define TAG "MAX30102"


MAX30102& MAX30102::GetInstance(
    gpio_num_t sda_gpio,
    gpio_num_t scl_gpio,
    gpio_num_t int_gpio,
    i2c_port_t i2c_port,
    uint8_t addr
) {
    // 静态局部变量：第一次调用时初始化，后续调用直接返回
    // 注意：C++11及以上保证静态局部变量初始化的线程安全性
    static MAX30102 instance(sda_gpio, scl_gpio, int_gpio, i2c_port, addr);
    return instance;
}


MAX30102::MAX30102(gpio_num_t sda_gpio, gpio_num_t scl_gpio, gpio_num_t int_gpio, 
                    i2c_port_t i2c_port, uint8_t addr) : 
                    sda_gpio_(sda_gpio), 
                    scl_gpio_(scl_gpio),
                    int_gpio_(int_gpio), 
                    i2c_port_(i2c_port),
                    addr_(addr)
{
   // Initialize I2C
   i2c_master_bus_handle_t i2c_bus_handle = NULL;
    i2c_master_bus_config_t i2c_conf = {
      .i2c_port           = i2c_port_,
      .sda_io_num         = sda_gpio_,
      .scl_io_num         = scl_gpio_,
      .clk_source         = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt  = 7,
      .flags = 
      {
        .enable_internal_pullup = true,
      },
    };
    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_conf, &i2c_bus_handle));
    i2c_master_dev_handle_t max30102_i2c_dev;
    i2c_device_config_t dev_config = {
        .dev_addr_length  = I2C_ADDR_BIT_LEN_7,     // 7位地址
        .device_address   = addr_,                  // 设备地址0x57
        .scl_speed_hz     = 100 * 1000,             // 100kHz
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_config, &max30102_i2c_dev));
    ESP_LOGI(TAG, "I2C Dev创建成功");
    if (max30102_i2c_dev == NULL)
      ESP_LOGI(TAG, "设备为空");
    max30102_dev = (max30102_dev_t *)calloc(1, sizeof(max30102_dev_t)); // 分配内存
    if (max30102_dev == NULL)
      ESP_LOGI(TAG, "内存分配失败");
    max30102_dev->dev_addr = addr_;                 // 设置设备地址
    max30102_dev->i2c_dev = max30102_i2c_dev;
    max30102_dev->i2c_bus = i2c_bus_handle;
    data = (max30102_data_t*)calloc(1, sizeof(max30102_data_t));
    if (data == NULL) {
        ESP_LOGE(TAG, "data 内存分配失败！");
        return;  // 分配失败时，后续避免使用 data 指针
    }
}

MAX30102::~MAX30102()
{
    if (max30102_dev != NULL)
        free(max30102_dev);
    if (data != NULL)
        free(data);
}



/**
 * @brief 读取 MAX30102 数据的函数
 * 
 * @param dev MAX30102 设备句柄
 * @param reg_addr 寄存器地址
 * @param data_buf 数据缓冲区
 * @param len 读取数据长度
 * @return esp_err_t 操作结果
 */
esp_err_t MAX30102::max30102_read(max30102_dev_t *dev, uint8_t reg_addr, uint8_t *data_buf, const uint8_t len)
{
  if (dev == NULL)
    ESP_LOGI(TAG, "设备为空");
  if (data_buf == NULL)
    ESP_LOGI(TAG, "缓冲区为空");
  if (len == 0)
    ESP_LOGI(TAG, "长度为0");
ESP_RETURN_ON_FALSE(dev != NULL && data_buf != NULL && len > 0, 
                        ESP_ERR_INVALID_ARG, TAG, "无效参数");
   // 第一步：发送寄存器地址（写操作）
    esp_err_t ret = i2c_master_transmit(
        dev->i2c_dev,          // 设备句柄（新版驱动要求的第一个参数）
        &reg_addr, 1,          // 寄存器地址，长度1字节
        500 / portTICK_PERIOD_MS  // 超时时间
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送寄存器地址失败：reg=0x%02x, err=0x%x", reg_addr, ret);
        return ret;
    }

    // 第二步：读取数据（读操作）
    ret = i2c_master_receive(
        dev->i2c_dev,          // 设备句柄
        data_buf, len,         // 接收缓冲区和长度
        500 / portTICK_PERIOD_MS  // 超时时间
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C读失败：reg=0x%02x, err=0x%x", reg_addr, ret);
    }
    return ret;
}
 
/**
 * @brief 写入 MAX30102 数据的函数
 * 
 * @param dev MAX30102 设备句柄
 * @param reg_addr 寄存器地址
 * @param data_buf 数据缓冲区
 * @param len 写入数据长度
 * @return esp_err_t 操作结果
 */
esp_err_t MAX30102::max30102_write(max30102_dev_t *dev, const uint8_t reg_addr, const uint8_t *const data_buf, const uint8_t len)
{
ESP_RETURN_ON_FALSE(dev != NULL && data_buf != NULL && len > 0, 
                        ESP_ERR_INVALID_ARG, TAG, "无效参数");

    // 合并寄存器地址和数据（寄存器地址在前，数据在后）
    uint8_t tx_buf[1 + len];
    tx_buf[0] = reg_addr;
    memcpy(tx_buf + 1, data_buf, len);

    // 发送数据（包含寄存器地址）
    esp_err_t ret = i2c_master_transmit(
        dev->i2c_dev,          // 设备句柄（匹配函数的第一个参数）
        tx_buf, 1 + len,       // 总数据长度（地址+数据）
        500 / portTICK_PERIOD_MS  // 超时时间
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C写失败：reg=0x%02x, err=0x%x", reg_addr, ret);
    }
    return ret;
}
 
/**
 * @brief 配置 MAX30102 设备
 * 
 * @param sensor MAX30102 句柄
 * @return esp_err_t 操作结果
 */
esp_err_t MAX30102::max30102_config(void)
{
    max30102_dev_t *dev = max30102_dev; // 类型转换
 
    esp_err_t ret;
    uint8_t data;
    data = (0x2 << 5) | (1 << 2); 
    ret = max30102_write(dev, MAX30102_REG_FIFO_CONFIG, &data, 1); // 写入FIFO配置寄存器
    assert(ret == ESP_OK);
    data = 0x03; // 设置模式为红光和红外样本
    ret = max30102_write(dev, MAX30102_REG_MODE_CONFIG, &data, 1); // 写入模式配置寄存器
    assert(ret == ESP_OK);
    data = (0x3 << 5) + (0x3 << 2) + 0x3; // 设置采样率和其他配置
    ret = max30102_write(dev, MAX30102_REG_SPO2_CONFIG, &data, 1); // 写入SPO2配置寄存器
    assert(ret == ESP_OK);
    data = 0xd0; // 设置红外脉冲功率
    ret = max30102_write(dev, MAX30102_REG_LED1_PA, &data, 1); // 写入LED1功率寄存器
    assert(ret == ESP_OK);
    data = 0xa0; // 设置红光脉冲功率
    ret = max30102_write(dev, MAX30102_REG_LED2_PA, &data, 1); // 写入LED2功率寄存器

    ESP_LOGI(TAG, "MAX30102配置成功");
    return ret;
}
 


esp_err_t MAX30102::max30102_deconfig(void) 
{
    max30102_dev_t *dev = max30102_dev;
    if (dev == nullptr) {
        ESP_LOGE(TAG, "传感器设备未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;
    uint8_t data;

    // 1. 停止测量：切换到待机模式（0x00）
    data = 0x00;
    ret = max30102_write(dev, MAX30102_REG_MODE_CONFIG, &data, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "切换到待机模式失败");
        return ret;
    }

    // 2. 关闭LED功率（设置为0）
    data = 0x00; // 红外LED功率设为0
    ret = max30102_write(dev, MAX30102_REG_LED1_PA, &data, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "关闭红外LED失败");
        return ret;
    }

    data = 0x00; // 红光LED功率设为0
    ret = max30102_write(dev, MAX30102_REG_LED2_PA, &data, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "关闭红光LED失败");
        return ret;
    }

    // 3. 重置FIFO缓冲区（可选，根据需求）
    data = 0x00; // 清除FIFO配置（恢复默认）
    ret = max30102_write(dev, MAX30102_REG_FIFO_CONFIG, &data, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重置FIFO失败");
        return ret;
    }

    ESP_LOGI(TAG, "MAX30102已取消配置（进入待机模式）");
    return ESP_OK;
}
/**
 * @brief 反初始化 MAX30102 设备
 * 
 * @param sensor MAX30102 句柄
 * @return esp_err_t 操作结果
 */
esp_err_t MAX30102::max30102_deinit(max30102_handle_t sensor)
{
    ESP_RETURN_ON_FALSE(sensor != NULL, ESP_ERR_INVALID_ARG, TAG, "无效的传感器句柄"); // 检查句柄是否为空
    max30102_dev_t *dev = (max30102_dev_t *) sensor; // 类型转换
    free(dev); // 释放内存
    return ESP_OK;
}
/**
 * @brief 获取 MAX30102 数据
 * 
 * @param sensor MAX30102 句柄
 * @param data 数据结构体指针
 * @return esp_err_t 操作结果
 */
esp_err_t MAX30102::max30102_read_data(void)
{
    ESP_RETURN_ON_FALSE(max30102_dev != NULL, ESP_ERR_INVALID_ARG, TAG, "无效的传感器句柄");    // 检查句柄是否为空
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "无效的数据指针");      // 检查数据指针是否为空

    max30102_dev_t *dev = max30102_dev; // 类型转换
    static int j = 0;
    esp_err_t ret = ESP_OK;
    uint8_t rptr, wptr = 0;
    int samp;
    uint8_t reg_data[256] = {0};
    ret = max30102_read(dev, MAX30102_REG_FIFO_WRITE_POINTER, &wptr, 1); // 读取写指针
    assert(ret == ESP_OK);
    ret = max30102_read(dev, MAX30102_REG_FIFO_READ_POINTER, &rptr, 1); // 读取读指针
    samp = ((32 + wptr) - rptr) % 32; // 计算样本数量
    if (samp <= 0) {
        ESP_LOGD(TAG, "FIFO无新数据，直接返回");
        data->hand_detected = false;
        data->heart_rate = 0;
        data->spo2 = 0;
        return ESP_FAIL; // 跳过无效读取
    }
    ret = max30102_read(dev, MAX30102_REG_FIFO_DATA_REGISTER, reg_data, 6 * samp); // 读取FIFO数据

    
    for (int i = 0; i < samp; i++) {
        dev->meastime += 0.01;
 
        dev->firxv[0] = dev->firxv[1];
        dev->firxv[1] = dev->firxv[2];
        dev->firxv[2] = dev->firxv[3];
        dev->firxv[3] = dev->firxv[4];
        //(1/3.48311) 系数 2048->587
        dev->firxv[4] = (1 / 3.48311) * (256 * 256 * (reg_data[6 * i + 3] % 4) + 256 * reg_data[6 * i + 4] + reg_data[6 * i + 5]);
 
        dev->firyv[0] = dev->firyv[1];
        dev->firyv[1] = dev->firyv[2];
        dev->firyv[2] = dev->firyv[3];
        dev->firyv[3] = dev->firyv[4];
        dev->firyv[4] = (dev->firxv[0] + dev->firxv[4]) - 2 * dev->firxv[2] + (-0.1718123813 * dev->firyv[0]) + (0.3686645260 * dev->firyv[1]) + (-1.1718123813 * dev->firyv[2]) + (1.9738037992 * dev->firyv[3]);
 
        dev->fredxv[0] = dev->fredxv[1];
        dev->fredxv[1] = dev->fredxv[2];
        dev->fredxv[2] = dev->fredxv[3];
        dev->fredxv[3] = dev->fredxv[4];
        dev->fredxv[4] = (1 / 3.48311) * (256 * 256 * (reg_data[6 * i + 0] % 4) + 256 * reg_data[6 * i + 1] + reg_data[6 * i + 2]);
 
        dev->fredyv[0] = dev->fredyv[1];
        dev->fredyv[1] = dev->fredyv[2];
        dev->fredyv[2] = dev->fredyv[3];
        dev->fredyv[3] = dev->fredyv[4];
        dev->fredyv[4] = (dev->fredxv[0] + dev->fredxv[4]) - 2 * dev->fredxv[2] + (-0.1718123813 * dev->fredyv[0]) + (0.3686645260 * dev->fredyv[1]) + (-1.1718123813 * dev->fredyv[2]) + (1.9738037992 * dev->fredyv[3]);
 
        // 检测心跳和血氧饱和度
        if (-1.0 * dev->firyv[4] >= 100 && -1.0 * dev->firyv[2] > -1 * dev->firyv[0] && -1.0 * dev->firyv[2] > -1 * dev->firyv[4] && dev->meastime - dev->lastmeastime > 0.5) {
            dev->hrarray[dev->hrarraycnt % 5] = 60 / (dev->meastime - dev->lastmeastime); // 计算心率
            dev->spo2array[dev->hrarraycnt % 5] = 110 - 25 * ((dev->fredyv[4] / dev->fredxv[4]) / (dev->firyv[4] / dev->firxv[4])); // 计算血氧饱和度
 
            if (dev->spo2array[dev->hrarraycnt % 5] > 100) {
                dev->spo2array[dev->hrarraycnt % 5] = 99.9; // 限制最大血氧饱和度
            }
            dev->lastmeastime = dev->meastime; // 更新最后一次测量时间
            dev->hrarraycnt++; // 心率计数器增加
 
            data->heart_rate = (dev->hrarray[0] + dev->hrarray[1] + dev->hrarray[2] + dev->hrarray[3] + dev->hrarray[4]) / 5; // 平均心率
            if (data->heart_rate < 40 || data->heart_rate > 150) {
                data->heart_rate = 0; // 心率异常则归零
            }
            data->spo2 = (dev->spo2array[0] + dev->spo2array[1] + dev->spo2array[2] + dev->spo2array[3] + dev->spo2array[4]) / 5; // 平均血氧饱和度
            if (data->spo2 < 50 || data->spo2 > 101) {
                data->spo2 = 0; // 血氧饱和度异常则归零
            }
            data->hand_detected = true; // 检测到手
            if (j == 10)
            {
                data->heart_rate = data->heart_s / 10.0;
                data->spo2       = data->spo2_s / 10.0;
                j = 0;
                data->heart_s = 0;
                data->spo2_s = 0;
                return ESP_OK;
            }
            j++;
            data->heart_s += data->heart_rate;
            data->spo2_s += data->spo2;
            return ESP_FAIL;
        } else {
            data->hand_detected = false; // 未检测到手
            data->heart_rate = 0; // 心率归零
            data->spo2 = 0; // 血氧饱和度归零
        }
    }
    return ESP_FAIL;
}

int MAX30102::Max30102_Get_Heart(void)
{
    return data->heart_rate;
}

int MAX30102::Max30102_Get_SpO2(void)
{
    return data->spo2;
}
