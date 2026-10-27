# AutoSense Guard

**C++17 车端传感器数据采集、TCP 通信与异常诊断练习项目**

> v1.0 当前通过模拟数据演示软件数据链路，不连接真实汽车，也不是经过车规认证的安全系统。文中的阈值仅为练习用示例值，不可用于真实车辆安全判断。

## 1. 项目功能

- **传感器模拟客户端**：生成车速、电压、温度、SOC、三轴加速度等模拟数据。
- **TCP 数据传输**：客户端通过 TCP 将一条样本一行地发送给本机服务器。
- **协议解析**：验证每个数据包是否包含规定的 9 个字段，并拒绝格式错误或非有限浮点数。
- **异步处理**：网络接收线程将数据放入有界线程安全队列，另一个处理线程执行诊断和日志写入。
- **异常诊断**：检测温度、电压、车速、SOC、IMU 加速度异常，以及序号跳变/倒退和时间戳倒退。
- **CSV 日志**：保存原始字段、诊断等级和问题描述，便于后续分析。
- **自动测试**：测试正常样本、温度告警、无效 SOC、序号缺失和协议解析。
- **故障注入**：模拟客户端可有意发送异常温度、无效 SOC 和序号间隙。

## 2. 项目结构

```text
AutoSenseGuard/
├── CMakeLists.txt
├── README.md
├── include/autosense/
│   ├── bounded_queue.hpp
│   ├── csv_logger.hpp
│   ├── diagnostic.hpp
│   ├── protocol.hpp
│   └── types.hpp
├── src/
│   ├── csv_logger.cpp
│   ├── diagnostic.cpp
│   └── protocol.cpp
├── apps/
│   ├── autosense_server.cpp
│   └── sensor_simulator.cpp
└── tests/
    └── diagnostic_test.cpp
```

## 3. Mac 编译

先在终端检查编译工具：

```bash
clang++ --version
cmake --version
```

如果没有 CMake，可通过 Homebrew 安装：

```bash
brew install cmake
```

进入项目目录后执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

如果测试通过，会看到 `100% tests passed`。

## 4. 运行演示

**终端 A：启动服务器**

```bash
./build/autosense_server 9000
```

**终端 B：发送 30 条正常模拟数据**

```bash
./build/sensor_simulator 127.0.0.1 9000 30 500
```

**或者，打开故障注入，演示告警和错误数据识别：**

```bash
./build/sensor_simulator 127.0.0.1 9000 30 300 --faults
```

参数依次为：服务器地址、端口、发送条数、发送间隔（毫秒）和可选的 `--faults`。服务器默认只监听 `127.0.0.1`，即本机回环地址，不会直接暴露到局域网。

服务器收到数据后，会在终端显示 `NORMAL`、`WARNING` 或 `CRITICAL` 结果，并把数据追加到：

```text
logs/sensor_data.csv
```

服务器运行期间按 `Control + C` 停止。再次运行会继续追加日志；如需全新测试，请先备份或删除该 CSV 文件。

## 5. 数据协议

每条样本以换行符结尾，字段按以下顺序排列：

```text
sequence,timestamp_ms,speed_kmh,voltage_v,temperature_c,soc,accel_x,accel_y,accel_z
```

例如：

```text
1,1792220000000,55.000,365.000,35.000,80,0.100,0.200,9.810
```

本项目采用简单 CSV 行协议，目的是练习 Socket、数据边界和错误处理；它不是汽车行业标准通信协议。后续可增加 CAN 帧或 ROS 2 消息适配层。

## 6. 诊断规则（练习阈值）

| 字段 | 示例规则 |
|---|---|
| 车速 | 小于 0 或大于 250 km/h：严重异常；大于 160 km/h：警告 |
| 电压 | 不在 200–500 V 范围内：严重异常；不在 280–430 V 范围内：警告 |
| 温度 | 大于 60 °C：严重告警；45–60 °C：警告 |
| SOC | 不在 0–100%：严重异常；不大于 10%：低电量警告 |
| 序号 | 重复或倒退：严重异常；跳号：警告 |
| 时间戳 | 不递增：警告 |
| IMU | 非有限数值：严重异常；加速度绝对值大于 20 m/s²：警告 |

真实汽车不同电池包、车型和传感器的量程及安全阈值并不相同，应以硬件规格和系统安全需求为准。

## 7. 下一阶段可扩展方向

1. 增加单元测试，覆盖协议截断、超大数据包、边界值和异常序号。
2. 增加接收速率、处理耗时、丢包数等性能统计，并编写可重复的压测脚本。
3. 增加配置文件，不把阈值硬编码在源文件中。
4. 通过适配器接入真实 CAN 设备或 CAN 日志回放。
5. 学习 ROS 2，增加 Topic 发布/订阅桥接。
6. 增加 Linux 下的 AddressSanitizer/ThreadSanitizer 检查。

## 8. 简历描述（项目完成测试后再使用）

**AutoSense Guard｜基于 C++ 的车端传感器采集与异常诊断系统**

- 使用 C++17 实现传感器模拟客户端与 TCP 接收服务端，定义并解析带序号和时间戳的数据协议。
- 设计有界线程安全队列，将网络接收与样本诊断/日志写入解耦。
- 实现传感器值域、温度等级、序号连续性及时间戳异常检测，输出 CSV 诊断日志。
- 使用 CMake 构建工程，并通过自动化测试验证主要协议与诊断规则。

请根据你实际完成和验证的内容修改上述简历描述，不要填写没有测量过的性能数字。
## 9. Roadmap

后续计划见 [ROADMAP.md](ROADMAP.md)。
