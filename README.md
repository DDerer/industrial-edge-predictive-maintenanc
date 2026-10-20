# 工业设备预测性维护边缘终端

这是一个基于 **STM32F401RE + Orange Pi 5B** 的工业设备状态监测项目。STM32
负责确定性数据采集，香橙派负责可靠通信、信号处理、故障诊断、数据存储和工业网络接入。

项目以小型直流电机或风扇作为被监测设备，通过人为增加偏心配重、负载和堵转来构造故障，
最终实现振动、电流、转速和温度的实时监测与故障预警。

## 1. 项目目标

- STM32 以固定采样率采集三轴振动、电流、转速和温度。
- 使用 DMA 双缓冲和 FreeRTOS，减少采样抖动与数据丢失。
- STM32 与香橙派之间使用带帧序号和 CRC16 的二进制协议。
- 香橙派运行 C++ 常驻服务，完成串口接收、流式拆包、断线恢复和数据质量统计。
- 对振动数据执行 FFT，并提取 RMS、峰值、峭度、峰值因子等特征。
- 使用传统算法和轻量神经网络识别不平衡、松动、过载等状态。
- 通过 SQLite 保存设备状态和报警，通过 Web 展示波形、频谱和历史趋势。
- 提供 MQTT 和 OPC UA 接口，通过 systemd 管理服务。
- 支持远程升级 STM32 固件，并完成连续运行稳定性测试。

## 2. 系统架构

```text
振动/电流/温度/转速
          │
          ▼
 STM32F401RE
 传感器驱动 → DMA双缓冲 → FreeRTOS任务 → 协议封包/CRC
          │ UART（原型）/ RS485（最终版本）
          ▼
 Orange Pi 5B
 串口采集服务 → 数据队列 → 特征提取/FFT → 故障诊断
          │                  │
          ▼                  ▼
 SQLite/时序数据          MQTT / OPC UA
          │
          ▼
 Web实时波形、频谱、报警和设备管理
```

## 3. 技术路线

### STM32 固件

- STM32CubeMX、HAL、FreeRTOS
- SPI + DMA 采集三轴加速度
- ADC + 定时器触发 + DMA 采集电流
- 定时器输入捕获测量转速
- UART DMA，后期增加 MAX3485 实现 RS485
- 独立看门狗、传感器超时检测、通信统计
- Bootloader、固件版本管理和 CRC 校验

### Linux 应用

- C++20、CMake
- `termios` 配置串口，流式解析器处理粘包、拆包和噪声
- 线程安全队列与环形缓冲区
- FFT 与时域/频域特征提取
- Python 训练模型，ONNX Runtime 或 RKNN 执行推理
- SQLite、MQTT、OPC UA、REST/WebSocket
- systemd、journald、配置文件和健康检查

## 4. 通信协议 v1

所有多字节整数均采用小端序。

| 字段 | 长度 | 说明 |
|---|---:|---|
| Magic | 2 | 固定为 `0xAA 0x55` |
| Version | 1 | 当前为 `1` |
| Type | 1 | 消息类型 |
| Payload length | 2 | 负载字节数，最大 4096 |
| Sequence | 4 | 递增帧序号，用于检测丢包 |
| Timestamp | 4 | STM32 启动后的毫秒数 |
| Payload | N | 消息负载 |
| CRC16 | 2 | CCITT-FALSE，计算范围为 Version 至 Payload |

首批消息类型：

| 类型 | 名称 | Payload |
|---:|---|---|
| `0x10` | Device status | 温度、母线电流、转速、状态标志 |
| `0x11` | Vibration batch | 采样率、样本数、连续的 XYZ 三轴 `int16` 样本 |
| `0x20` | Control command | 命令编号及参数，后续实现 |
| `0x21` | Command response | 命令执行结果，后续实现 |

CRC参数：多项式 `0x1021`，初始值 `0xFFFF`，不反射，结果不异或。

## 5. 实施阶段

### 阶段一：协议与Linux采集雏形（当前）

- [x] 定义协议帧格式
- [x] 实现 CRC16
- [x] 实现能够抵抗拆包、粘包和噪声的 C++ 流式解析器
- [x] 实现虚拟 STM32 数据生成器
- [x] 完成端到端本机验证
- [ ] 接入 Orange Pi UART
- [ ] 接入 STM32F401RE UART DMA

### 阶段二：真实传感器采集

- [ ] 选择并接入三轴加速度传感器
- [ ] 固定采样率和 DMA 双缓冲
- [ ] 接入电流、转速和温度传感器
- [ ] 测量采样抖动、吞吐量和丢包率

### 阶段三：信号处理与故障诊断

- [ ] 时域特征与 FFT
- [ ] 建立正常、不平衡、松动、过载数据集
- [ ] 阈值基线算法
- [ ] 训练并部署轻量异常检测模型
- [ ] 统计准确率、误报率和报警延迟

### 阶段四：工业网关与工程化

- [ ] SQLite、WebSocket 与可视化页面
- [ ] MQTT 和 OPC UA
- [ ] systemd、日志、自动重连和看门狗
- [ ] STM32 远程固件升级
- [ ] 24小时及以上稳定性测试

## 6. 当前目录

```text
linux_project/
├── README.md
├── CMakeLists.txt
├── firmware/             # STM32F401RE 固件
├── gateway/              # Orange Pi 5B C++ 采集服务和测试
├── web/                  # 监控后台与页面
├── config/               # 网关配置
├── deploy/orangepi5b/    # systemd 与部署文件
├── tools/
│   └── stm32_simulator.py
└── scripts/
    └── demo.sh
```

## 7. 构建与运行

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./scripts/demo.sh
```

直接读取串口：

```bash
./build/gateway/edge_collector --input /dev/ttyS4 --baud 115200
```

当前版本把解析结果打印到终端。下一步会把数据送入内部队列，并加入特征提取和持久化。

