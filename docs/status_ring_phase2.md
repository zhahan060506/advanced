# WS2812B 24 灯珠状态灯环：第二阶段

更新：2026-10-19。代码和编译期逻辑检查已完成；尚未烧录或验证真实灯环。

## 当前配置

板型为 bread-compact-wifi-lcd。配置位于 main/boards/bread-compact-wifi-lcd/config.h：

- STATUS_RING_ENABLED = 1
- STATUS_RING_GPIO = GPIO_NUM_13
- STATUS_RING_LED_COUNT = 24
- STATUS_RING_BRIGHTNESS = 24（每个颜色通道的最大输出，范围 1～32，满量程 255）
- STATUS_RING_STARTUP_TEST = 1

采用 WS2812、GRB 格式。具体灯环型号和颜色顺序还需实物确认。

## 灯效与优先级

| 状态 | 灯效 |
| --- | --- |
| 上电测试 | 红、绿、蓝各约 350ms |
| 待机 | 熄灭 |
| 监听 / 音频测试 | 蓝色 |
| 回答 | 绿色 |
| 倒计时运行 | 黄色 |
| 倒计时或闹钟响铃 | 红色，亮灭各 250ms |
| 应用错误提示 | 红色快闪，约 1.5 秒后恢复当前状态 |
| 致命错误状态 | 持续红色快闪 |
| 启动 / 配网 | 蓝色慢闪 |
| 连接 | 青色 |
| 升级 | 紫色快闪 |
| 激活 | 青色闪烁 |

优先级：响铃 > 错误 > 监听/回答 > 倒计时 > 其他设备状态。
因此倒计时期间可以正常显示对话灯色，对话结束恢复黄色；响铃取消或结束后重新显示当前实际状态。仅设置未来闹钟不会点亮倒计时黄灯。
上电测试会在响铃、错误、语音、倒计时或升级开始时提前停止。

## 接线前确认

用户已提供 24 灯珠灯环背面照片，输入端黑线焊 GND、黄线焊 DI、红线焊 5V；另一侧为 GND、DO、5V。
当前小电视由 USB 供电，用户以后可能改用充电头，尚未确认外部 5V 电源。用户决定暂缓灯环实测、先开发舵机；以下接线是待落实方案。

针对标准 5V WS2812B RGB 灯环，计划如下：

| 接点 | 连接方案 |
| --- | --- |
| 灯环 5V / VCC | 外部稳压 5V；不得直接接 12V |
| 灯环 GND | 外部电源负极，并与 ESP32 GND 共地 |
| 灯环 DIN | GPIO13 经合适的 3.3V→5V 电平转换后，再串联约 330Ω 电阻接入 |
| 灯环 DOUT | 单个灯环时不接 |

5V WS2812 灯珠与 3.3V MCU 配合时，推荐 74AHCT125 或 74HCT245 等适合的电平转换器。330Ω 电阻不能替代电平转换。具体转换器引脚、使能脚及去耦应按所购器件确认。
在灯环供电入口可加 500～1000µF、耐压至少 6.3V 的电容，注意极性；例如 1000µF/10V。数据电阻靠近 DIN。
接线前断电；ESP32 使用 USB 供电时，外部 5V 电源暂只给灯环供电，与 ESP32 共地，不将两路 5V 直接并联。
低软件亮度不代替电源容量核实。确认电源板额定输出后再上电。

参考：[Adafruit NeoPixel 接线实践](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices)、[NeoPixel 供电说明](https://learn.adafruit.com/adafruit-neopixel-uberguide/powering-neopixels)。

## 编译与烧录

在 VS Code 的 ESP-IDF 终端中，进入 D:\shoubiaob：

```powershell
idf.py -B build-local build
idf.py -B build-local -p COM3 flash monitor
```

如实际端口变化，以设备管理器中的端口为准。退出串口监视使用 Ctrl+]。

本次完整编译通过，固件大小 5,111,824 字节，app 分区剩余 19%。
tests/status_light_policy_test.cc 的编译期断言已通过，检查优先级、恢复状态、闪烁边界、颜色及亮度限制。完整构建日志为 build-local/status-ring-build.log。

## 实物验收顺序

1. 检查共地、供电电压、DIN 方向及转换器接法后，上电观察全部 24 颗灯的红→绿→蓝测试。若被活动状态打断，可待设备空闲后重启检查。
2. 串口查看 StatusLight 日志和 Driver ready 信息。驱动初始化成功只能证明软件初始化完成，不能证明灯环真实收到信号。
3. 空闲时熄灭；唤醒并监听时蓝色，回答时绿色。
4. 启动倒计时显示黄色；倒计时期间对话显示语音颜色，对话结束恢复黄色。
5. 倒计时或闹钟到时红闪，取消或自然结束后恢复当前状态。
6. 回归测试屏幕、按键、语音和原有定时功能。记录颜色异常、闪烁、重启或音频中断及对应串口日志。

手势模块暂不接线也可测试灯环；手势初始化失败应只产生诊断日志。当前手势功能仍限于日志识别，尚未控制灯环或页面。

## 主要代码位置

- main/status_light_controller.h / .cc：独立任务、RMT 输出、启动测试与错误提示期限。
- main/status_light_policy.h：状态优先级与颜色规则。
- main/timer_controller.h / .cc：提供倒计时与响铃状态快照。
- main/application.cc：错误提示通知。
- main/boards/bread-compact-wifi-lcd/compact_wifi_board_lcd.cc：初始化灯环。

灯环代码修改前备份在 releases/gesture_log_before_status_ring_2026-10-19；原功能实测正常版本备份在 releases/baseline_before_gesture_2026-10-19。
