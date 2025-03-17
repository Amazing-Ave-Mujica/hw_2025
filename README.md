# DiskScheduler

## 基本设定
### 存储对象
标号，标签，大小，存储的硬盘标号

### 存储块
所属的存储对象，标号

### 磁盘
磁头, 磁盘, 每种对象对象块的位置 (哈希表), 读取队列，3 种操作对应的时间片花销，剩余时间片

可能需要实现的功能：

每一轮读取队列首的对象块，调度由上层负责

每次 read 后向 disk-manager 汇报，由 disk-manager 维护读队列，磁盘不能写

估算当前状态读完某个对象所需要花费的时间片，方便 disk-manager 安排插队

### 磁盘管理器
```cpp
struct DiskReadRequest {
  struct 存储对象;
  int timestamp;
  std::unordered_map<对象块， 磁盘> ump;
}
```

写入：
尽量找到一块最适合的连续空间存储

删除：
取消该对象未完成的读请求，及时更新磁盘的读队列

读：
按照一定方法，组织每个磁盘的读队列。磁盘每次读之后，及时更新队列

简单策略：

每次将某个对象的读请求均匀分布在三个副本上。

每次时间片都对每个磁盘的读顺序进行规划，比如先进先出等策略

每次由磁盘规划出磁头移动到特定位置的最优方案

可能的优化：
磁盘可能读到计划外的存储块，由于存储基本连续，可以考虑插队