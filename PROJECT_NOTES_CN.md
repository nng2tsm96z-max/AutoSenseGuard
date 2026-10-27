# 开发说明（给初学者）

建议先按这个顺序阅读源码：

1. `include/autosense/types.hpp`：每条传感器数据有哪些字段。
2. `src/protocol.cpp`：字符串数据如何转成 `SensorData`。
3. `src/diagnostic.cpp`：怎样根据字段判断告警等级。
4. `src/csv_logger.cpp`：怎样把数据和诊断结果保存到 CSV。
5. `include/autosense/bounded_queue.hpp`：为什么需要互斥锁、条件变量和有界队列。
6. `apps/sensor_simulator.cpp`：客户端怎样创建 TCP 连接并发送样本。
7. `apps/autosense_server.cpp`：服务端怎样接收、排队、诊断和记录数据。
8. `tests/diagnostic_test.cpp`：如何用测试确认关键规则。

建议你不要只复制项目。尝试自行修改温度阈值、加入一个新的传感器字段、添加对应测试，然后观察 CSV 中的输出。项目使用模拟数据，当前没有连接真实汽车硬件。
