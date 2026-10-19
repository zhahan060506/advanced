#include "weather.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "application.h"
#define TAG "Weather"
#define PUBLIC_IP_API_URL "https://api.seniverse.com/v3/weather/now.json?key=" XINZHI_KEY "&language=zh-Hans&unit=c&location=ip"  // 无密钥，直接返回IP

extern const char my_tianqi_cert_pem_start[] asm("_binary_my_tianqi_cert_pem_start");
extern const char my_tianqi_cert_pem_end[] asm("_binary_my_tianqi_cert_pem_end");

static char res_data[1024];
esp_err_t tianqi_event_handler(esp_http_client_event_t *evt)
{
    
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // 直接将数据复制到响应缓冲区
                if (evt->data_len < sizeof(res_data)) {
                    memcpy(res_data, evt->data, evt->data_len);
                    res_data[evt->data_len] = '\0'; // 确保字符串以null结尾
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

bool Weather::Get_Weather(void)
{
    esp_http_client_config_t config = 
    {
        .url = PUBLIC_IP_API_URL,
        .cert_pem = my_tianqi_cert_pem_start,
        .event_handler = tianqi_event_handler,        
    };
    client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) 
    {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
          cJSON *root = cJSON_Parse(res_data);
          if (root == nullptr) {
             return false;
          }
          cJSON *results = cJSON_GetObjectItem(root, "results");
          if (results == nullptr || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
              cJSON_Delete(root);
              return false;
          }
          cJSON *first_result = cJSON_GetArrayItem(results, 0);
          if (first_result == nullptr) {
              cJSON_Delete(root);
              return false;
          }
          cJSON *location = cJSON_GetObjectItem(first_result, "location");
          if (location == nullptr) {
              cJSON_Delete(root);
              return false;
          }
          
          cJSON *name = cJSON_GetObjectItem(location, "name");
          if (name == nullptr) {
               cJSON_Delete(root);
              return false;
              
          }
          cJSON *now = cJSON_GetObjectItem(first_result, "now");
          if (results == nullptr || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
              cJSON_Delete(root);
              return false;
          }
          cJSON *text = cJSON_GetObjectItem(now, "text");
          if (text == nullptr && !cJSON_IsString(text)) {
              cJSON_Delete(root);
              return false;
          }
          cJSON *temperature_json = cJSON_GetObjectItem(now, "temperature");
          if (temperature_json == nullptr && !cJSON_IsString(temperature_json)) {
              cJSON_Delete(root);
              return false;
          }
          cJSON *code = cJSON_GetObjectItem(now, "code");
          code_ = atoi(code->valuestring);
          strncpy(city, name->valuestring, sizeof(city)-1);
          city[sizeof(city)-1] = '\0';
          strncpy(weather, text->valuestring, sizeof(weather)-1);
          weather[sizeof(weather)-1] = '\0';
          strncpy(temperature, temperature_json->valuestring, sizeof(temperature)-1);
          temperature[sizeof(temperature)-1] = '\0';
          
          ESP_LOGI(TAG, "地区: %s 天气: %s 温度: %s code:%d",  city, weather, temperature, code_);

          cJSON_Delete(root);
          return true;
      } 
    }
    return false;
}
Weather::Weather(void)
{
}

Weather::~Weather()
{
  esp_http_client_cleanup(client);
}














