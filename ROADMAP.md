# Roadmap

## P0 — 本周

- [ ] 多客户端 TCP 服务：epoll、client ID、4-8 路连接
- [ ] 指标：received/parsed/dropped/processed、队列深度、p50/p95/p99
- [ ] 背压：try_push + DropOldest + 丢包计数

## P1 — 下周

- [ ] 协议 v2：magic/version/type/length/seq/timestamp
- [ ] 共享内存环形缓冲区 + TCP vs SHM 压测
- [ ] ROS2 bridge + ros2 bag 回放

## 已完成

- [x] 项目进入 Git，baseline 提交
- [x] 用 CHECK 替换 assert，Debug/Release 均验证