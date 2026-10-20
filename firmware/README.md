# NUCLEO-F401RE 固件

`include/edge_protocol.h` 与 `src/edge_protocol.c` 是不依赖 HAL 的协议 v1 编码器。它们生成的状态帧、振动帧已经通过主机测试与 `gateway` 的流式解析器互通。编码器不分配内存；调用方提供缓冲区，返回值为帧长度，`0` 表示参数或容量错误。

## 在 STM32CubeIDE 中使用

1. 为 NUCLEO-F401RE 创建 STM32CubeIDE 工程，启用 USART2 异步模式，115200、8N1，启用 USART2 TX DMA 和相应中断。
2. 本仓库已经包含 CubeMX 生成的工程 `NucleoF401RE/`，以及放在 `Core/Inc`、`Core/Src` 中的协议和演示模块。可用 STM32CubeIDE 导入该工程。
3. `main.c` 在外设初始化后调用 `edge_uart_demo_poll(&huart2)`，并在 HAL 回调中转发 DMA 完成和 UART 错误事件：

```c
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    edge_uart_demo_tx_complete(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    edge_uart_demo_error(huart);
}
```

4. 该演示每秒发送一帧虚拟状态数据，每约 32 ms 发送一帧虚拟振动数据。传输完成回调触发前，不能改写或释放 DMA 缓冲区；示例使用静态缓冲区并在传输中暂停编码。真实传感器接入后再替换虚拟数据。

状态帧需要 32 字节缓冲区。振动帧需要 `20 + 6 × 样本数` 字节，协议最多支持 682 组三轴样本。115200 8N1 的线速约为 11520 字节/秒，因此 1000 Hz 三轴 `int16` 原始数据约占 6000 字节/秒，仍需给状态帧、协议开销和调度留余量。

NUCLEO-F401RE 的板载 ST-LINK 提供虚拟串口；默认焊桥配置下，MCU 的 USART2 PA2/PA3 接到 ST-LINK。可先将 NUCLEO 的 ST-LINK USB 口接到 Orange Pi USB 口，通过 Orange Pi 枚举出的 `/dev/ttyACM*` 测试。板卡手册：[ST UM1724](https://www.st.com/resource/en/user_manual/dm00105823-nucleo-f401re-user-manual-stmicroelectronics.pdf)。实际设备名以系统枚举结果为准。

## 本机兼容性测试

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

STM32CubeIDE 工程已生成并接入演示模块；下载到板子仍需 ST-LINK 数据线。演示里的波形由软件生成，并不代表定时器驱动的真实 1000 Hz 采样。下一步是在 Orange Pi 上验证真实串口帧，再接入传感器和定时采样。
