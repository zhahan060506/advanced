# SG90 舵机：第三阶段

更新：2026-10-19。用户已拿到 SG90，软件第一版已编译通过；尚无接线、机械安装或动作实测结果。

## 当前交付

- main/servo_controller.h / .cc：50Hz LEDC PWM、独立低优先级任务、非阻塞目标角度接口、停止和可选语音工具。
- main/servo_motion.h：逻辑角度限制、脉宽和占空比换算、每步移动及轻摆规则。
- tests/servo_motion_test.cc：编译期逻辑检查，包括不同起点和目标下的范围、步长和收敛检查。
- 板级初始化与 CMake 已接入。舵机使用 GPIO14、LEDC timer 2 / channel 2，背光维持 timer 0 / channel 0。

完整构建通过，当前配置的应用固件 0x4e0240 字节，app 分区剩余 19%。日志：build-local/servo-build.log。
以上结果仅证明代码可编译及运动计算符合预期，不证明舵机实际角度、电流或机械负载满足要求。

## 默认关闭输出

main/boards/bread-compact-wifi-lcd/config.h 当前为：

```cpp
#define SERVO_ENABLED             0
#define SERVO_GPIO                GPIO_NUM_14
#define SERVO_STARTUP_TEST        0
#define SERVO_FOLLOW_SPEECH       0
```

保持以上配置时，不初始化舵机 PWM、不注册舵机语音工具；启动日志显示 Disabled in config。其他模块按各自配置运行。
先确认实际 SG90 接线、稳定电源及机械活动空间，再改变配置。当前小电视使用 USB 供电，后续可能使用充电头；外部电源方案尚未确定，不据此假设 USB 或主板供电能力足够带动舵机。

## 动作定义

- 接口 SetAngle(angle) 接受 60～120 的逻辑角度，超范围拒绝；Center() 等同于 SetAngle(90)。90 对应 1500µs 中位脉冲，允许范围约 1333～1667µs。
- 此映射是初始测试参数，不是实物角度标定；逻辑 80/100 哪个是向左，要看舵机和支架安装方向。
- 每 20ms 最多移动 1 个逻辑角度单位，实际任务调度可能使动作更慢。
- 启用后第一次输出中位脉冲。没有轴位置反馈，首次启动不能保证从原位置平滑回中；首次测试应让舵机空载、暂不连接屏幕连杆。
- 上电测试可选，顺序为 90→80→100→90，各阶段约 1 秒；测试结束保持中位。
- 说话轻摆可选，仅设备进入 Speaking 状态时，在目标附近 ±5 之间缓慢移动；说话结束恢复基础目标。它不是随声音幅度变化的口型同步。
- 手动设置角度后，取消当次启动测试和说话自动摆动，直到重启。避免语音回复时自动轻摆覆盖用户刚设置的姿态。
- Stop() 锁定停止 PWM，直到重启；它不会切断供电，也不保证舵机继续保持位置。工作任务在下一轮处理停止请求，通常约 20ms，非硬件急停。
- 初始化或输出失败仅记录日志并停用舵机，不主动终止电视应用。

## 后续实测步骤

1. 确认舵机型号、线序和电源，断电接线。信号预留 GPIO14，电源地须与 ESP32 共地；具体电源正极接法在供电确认后给出。
2. 先不挂屏幕负载。设置 SERVO_ENABLED=1、SERVO_STARTUP_TEST=1、SERVO_FOLLOW_SPEECH=0。
3. 在 ESP-IDF 终端运行：

   ```powershell
   idf.py -B build-local build
   idf.py -B build-local -p COM3 flash monitor
   ```

4. 观察一次中位、小范围两侧、回中的动作，检查供电重启、抖动、机械卡住和屏幕背光变化。PWM configured 日志只表示软件配置成功，不表示舵机连接或实际到位。
5. 开启后注册的 self.servo.set_angle 工具可用于语音请求“舵机转到80度”“舵机回到90度”；准确调用仍依赖现有 AI 服务。请求“停止舵机”调用 self.servo.stop，之后需重启才重新输出。工具返回 movement queued 只说明指令已接受。
6. 基础动作通过后，将 SERVO_STARTUP_TEST=0；完成安装和范围校准，再考虑 SERVO_FOLLOW_SPEECH=1，验证回答时轻摆、回答结束回到基础目标。
7. 最后再接入 PAJ7620 的左右手势；目前手势仍只打印日志。

实现参考：[ESP-IDF v5.4.4 ESP32-S3 LEDC 文档](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-reference/peripherals/ledc.html)。软件复用现有 IDF 驱动，无新增组件依赖。
