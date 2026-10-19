# GitHub 备份与恢复

仓库：https://github.com/zhahan060506/advanced

本次保存源码、项目文档、测试、sdkconfig 和 dependencies.lock。原有功能已由用户实测；新增手势、灯环、舵机代码已编译，硬件实测尚未完成。详细状态见 project_progress.md。

编译输出 build/、build-local/，下载依赖 managed_components/，本地 releases/ 备份、串口日志及电脑专用 .vscode/ 设置不上传。固件可以重新编译生成。

## 天气密钥

天气 API 密钥保存在本机 main/weather/weather_credentials.h，该文件被 Git 忽略。本次整理保留了原电脑的密钥，新电脑需要自行配置：

```powershell
Copy-Item main\weather\weather_credentials.example.h main\weather\weather_credentials.h
```

随后在 weather_credentials.h 中填入自己的心知天气 API 密钥。没有此文件时仍可编译，但天气 API 请求没有有效密钥，不能正常获取天气。不要将真实密钥写入示例文件或提交到 GitHub。

## 新电脑恢复

1. 安装 Git、VS Code 和 ESP-IDF v5.4.4。
2. 克隆仓库：git clone https://github.com/zhahan060506/advanced.git。
3. 在 VS Code 打开克隆目录，并配置新电脑上的 ESP-IDF 路径。
4. 配置上述天气密钥；保留仓库 sdkconfig 中当前板型、屏幕及分区设置。
5. 在 ESP-IDF 终端运行 idf.py -B build-local build。首次构建需联网下载锁定的组件。
6. 实际烧录端口按设备管理器确认；不要假定新电脑仍为 COM3。

设备 NVS 中的 Wi-Fi 配网及服务配置不包含在本仓库内。固件当前关闭舵机输出，启用与测试见 servo_phase3.md。
