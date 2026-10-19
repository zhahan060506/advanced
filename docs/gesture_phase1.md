# PAJ7620U2 第一阶段测试

本阶段在 bread-compact-wifi-lcd 板型中加入手势识别日志。
已完成本机 ESP-IDF v5.4.4 编译；尚未完成接线、烧录和手势实测。
用户已确认升级前的屏幕、语音、按键、倒计时、闹钟等基础功能正常。

2026-10-19 进度补充：用户已提供模块照片，确认丝印 GY-PAJ7620，
感应器朝前、焊孔在下时从左到右为 VIN、GND、SCL、SDA、INT。
已给出 VIN 先用 3.3V 测试的接线方案；照片中尚未焊接排针。
用户要求暂放这一阶段，先开发 24 颗 WS2812B 灯环；手势实测仍待完成。

## 当前改动

- `main/sensors/paj7620.h/.cc`：I2C 设备探测、芯片 ID 校验、寄存器初始化、读取手势。
- 板级 `config.h`：SDA=GPIO11、SCL=GPIO12、地址=0x73、轮询间隔=200ms。
- 板级初始化：独立低优先级任务打印手势；不调用页面、音量、灯环或舵机控制。
- `main/CMakeLists.txt`：仅在当前板型编译该驱动。
- `.vscode/settings.json`：串口记录更新为用户已确认的 COM3。

模块缺失、芯片型号不符、初始化失败时，手势测试退出，应用继续启动。
读取连续失败五次时，任务释放设备及总线后退出。修复接线后重启设备重新检测。
这些是代码中的容错措施，仍需实物确认；本阶段不支持带电插拔模块。

## 接线前确认

先断开 USB 和其他供电。按照模块丝印连接，不按排针位置猜测。

| 模块引脚 | 接线 |
| --- | --- |
| SDA | ESP32-S3 GPIO11 |
| SCL | ESP32-S3 GPIO12 |
| GND | ESP32-S3 GND |
| VCC/VIN | 先确认该模块供电规格；支持 3.3V 的模块按原方案接 3.3V |
| INT | 本阶段不接 |

PAJ7620U2 是芯片型号，不足以确定所有成品模块的供电和电平转换方式。
若卖家只标注 5V 供电，先核对原理图或模块照片及规格，并确认 SDA/SCL 为
ESP32 可接受的 3.3V 电平。不要把 5V 上拉的 I2C 信号直接接到 ESP32。
GPIO19/20 保留给原生 USB；GPIO13/14 留待后续灯环和舵机阶段。
I2C 当前为 100kHz，启用芯片内部上拉；若模块没有合适的外部上拉，需按硬件规格补充。

## 编译和烧录

在项目根目录的 ESP-IDF 终端执行：

```powershell
idf.py -B build-local build
idf.py -B build-local -p COM3 flash monitor
```

若已有监视器占用 COM3，先按 Ctrl+] 退出。端口改变时使用设备管理器显示的实际端口。

## 预期日志与验收

初始化成功的日志示例（时间戳不固定）：

```text
I (...) PAJ7620: Log test: SDA=11 SCL=12, poll=200 ms
I (...) PAJ7620: Ready: address=0x73, chip=0x7620, mode=NEAR_240FPS
I (...) PAJ7620: Gesture: LEFT (raw=0x0004)
I (...) PAJ7620: Gesture: RIGHT (raw=0x0008)
I (...) PAJ7620: Gesture: FORWARD (raw=0x0010)
```

以上为示例，不是本机实测记录。没有识别结果时不会周期打印 NONE。
UNKNOWN 表示非单一手势的标志组合，保留原始值以便排查，不执行动作。

1. 确认 Ready 日志出现，屏幕、联网、语音依旧正常。
2. 固定模块方向，先在感应面前约 5～10cm 分别做上、下、左、右动作。
3. 分别测试靠近和远离感应面的动作，记录 FORWARD/BACKWARD 日志。
4. 每个方向先测试五次，每次动作之间把手移开并停顿约一秒。
5. 记录实际动作、打印方向、原始值及重复/漏识别情况；此阶段允许继续校准。
6. 如果方向与预期不同，记录模块安装方向后再统一映射，先不要交换接线。

当前方向名称遵循 Seeed 2023 版库；模块安装角度会影响物理方向。
本阶段输出直接识别结果，尚未增加复合手势消歧、冷却或控制动作。
达到稳定识别后，再进入页面和音量控制阶段。

常见异常：

- `No response at 0x73`：断电检查 VCC、GND、SDA、SCL 和模块型号。
- `Unexpected chip ID`：地址响应但芯片 ID 不符；确认买到的是 PAJ7620U2。
- `Five consecutive read errors`：检查接触、供电和 I2C 上拉，断电修复后重启。
- 没有任何 PAJ7620 日志：确认烧录的是本阶段固件，并在监视器打开后复位以查看启动日志。

## 编译记录和回退

2026-10-19 本机编译通过，固件 5,108,928 字节（0x4DF4C0），
6MiB app 分区剩余 1,182,528 字节，构建工具显示 19%。
相对用户刚验证的 5,100,208 字节基础固件，增加 8,720 字节。
详细日志：`build-local/gesture-build.log`。

升级前源码与烧录文件已保存至：
`releases/baseline_before_gesture_2026-10-19/`。
暂时停用手势功能可将板级 `config.h` 的 `GESTURE_SENSOR_ENABLED` 改为 0 后重新编译烧录。

## 来源

寄存器表与初始化顺序参考 Seeed 的 MIT 许可实现，许可证随驱动保存：

- https://github.com/Seeed-Studio/Grove_Gesture/blob/master/src/paj7620.h
- https://github.com/Seeed-Studio/Grove_Gesture/blob/master/src/Gesture.cpp
- https://wiki.seeedstudio.com/Grove-Gesture_v1.0/

开发 API 依据本机 ESP-IDF v5.4.4 的 `driver/i2c_master.h`。
