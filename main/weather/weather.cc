#include "weather.h"
#include "bounded_response.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <esp_log.h>

namespace {
constexpr const char* kTag = "Weather";
constexpr const char* kWeatherUrl = "https://api.seniverse.com/v3/weather/now.json?key="
    XINZHI_KEY "&language=zh-Hans&unit=c&location=ip";
using Response = BoundedResponse<2048>;

esp_err_t OnHttpEvent(esp_http_client_event_t* event) {
    if (event->event_id == HTTP_EVENT_ON_DATA) {
        auto* response = static_cast<Response*>(event->user_data);
        if (response == nullptr || event->data_len < 0 ||
            !response->Append(static_cast<const char*>(event->data), event->data_len)) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}
struct HttpGuard {
    esp_http_client_handle_t handle;
    ~HttpGuard() { if (handle) esp_http_client_cleanup(handle); }
};
}  // namespace

extern const char my_tianqi_cert_pem_start[] asm("_binary_my_tianqi_cert_pem_start");

bool Weather::Get_Weather() {
    if (XINZHI_KEY[0] == '\0') {
        ESP_LOGW(kTag, "No local weather API key; skipping request");
        return false;
    }
    Response response;
    esp_http_client_config_t config = {};
    config.url = kWeatherUrl;
    config.cert_pem = my_tianqi_cert_pem_start;
    config.timeout_ms = 5000;
    config.event_handler = OnHttpEvent;
    config.user_data = &response;
    HttpGuard client{esp_http_client_init(&config)};
    if (!client.handle) return false;
    const esp_err_t err = esp_http_client_perform(client.handle);
    if (err != ESP_OK || response.Failed() || response.Size() == 0 ||
        esp_http_client_get_status_code(client.handle) != 200) {
        ESP_LOGW(kTag, "Weather request failed or response incomplete");
        return false;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_Parse(response.Data()), cJSON_Delete);
    const auto* results = cJSON_GetObjectItemCaseSensitive(root.get(), "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) return false;
    const auto* first = cJSON_GetArrayItem(results, 0);
    const auto* location = cJSON_GetObjectItemCaseSensitive(first, "location");
    const auto* now = cJSON_GetObjectItemCaseSensitive(first, "now");
    if (!cJSON_IsObject(location) || !cJSON_IsObject(now)) return false;
    const auto* name = cJSON_GetObjectItemCaseSensitive(location, "name");
    const auto* text = cJSON_GetObjectItemCaseSensitive(now, "text");
    const auto* value = cJSON_GetObjectItemCaseSensitive(now, "temperature");
    const auto* code = cJSON_GetObjectItemCaseSensitive(now, "code");
    if (!cJSON_IsString(name) || !cJSON_IsString(text) ||
        !cJSON_IsString(value) || !cJSON_IsString(code) || !code->valuestring[0]) return false;
    char* end = nullptr;
    errno = 0;
    const long parsed_code = std::strtol(code->valuestring, &end, 10);
    if (errno || *end != '\0' || parsed_code < 0 || parsed_code > 255) return false;
    // Commit only a completely validated response; retain last good data on failure.
    std::strncpy(city, name->valuestring, sizeof(city) - 1);
    std::strncpy(weather, text->valuestring, sizeof(weather) - 1);
    std::strncpy(temperature, value->valuestring, sizeof(temperature) - 1);
    city[sizeof(city) - 1] = weather[sizeof(weather) - 1] = temperature[sizeof(temperature) - 1] = '\0';
    code_ = static_cast<uint8_t>(parsed_code);
    ESP_LOGI(kTag, "Weather updated, code=%u", code_);
    return true;
}

Weather::Weather() = default;
Weather::~Weather() = default;
