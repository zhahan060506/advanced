#ifndef WEATHER_H
#define WEATHER_H
#include "esp_http_client.h"
#include "cJSON.h"
#include <string>
// Personal credentials stay local; a fresh checkout can build without them.
#if __has_include("weather_credentials.h")
#include "weather_credentials.h"
#endif
#ifndef XINZHI_KEY
#define XINZHI_KEY ""
#endif
class Weather
{
private:
  char city[64] = "--";
  char weather[32] = "--";
  char temperature[10] = "--";
  uint8_t code_ = 255;
public:
  bool Get_Weather(void);
  char* Get_City_Value(void)
  {
    return city;
  }
  char* Get_Weather_Value(void)
  {
    return weather;
  }
  char* Get_Temperature_Value(void)
  {
    return temperature;
  }
  uint8_t Get_Code(void)
  {
    return code_;
  }
  Weather(/* args */);
  ~Weather();
};


#endif // WEATHER_H
