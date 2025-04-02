#include "include/config.h"
#include "include/data.h"
#include "include/disk.h"
#include "include/disk_manager.h"
#include "include/init.h"
#include "include/object.h"
#include "include/printer.h"
#include "include/resource_allocator.h"
#include "include/scheduler.h"
#include "include/top_scheduler.h"
#include "include/tsp.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <numeric>
#include <thread>

int timeslice = 0;

auto main() -> int {

#ifdef LLDB
  freopen(R"(/home/fiatiustitia/HW-2025/data/sample_practice.in)", "r", stdin);
  freopen(R"(/home/fiatiustitia/HW-2025/log/code_craft.log)", "w", stdout);
#endif

  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  int t, m, n, v, g,k; // NOLINT
  std::cin >> t >> m >> n >> v >>
      g >> k; // 输入时间片数量、标签数量、磁盘数量、磁盘容量、生命周期

  config::RTQ_DISK_PART_SIZE = v / m;
  config::JUMP_THRESHOLD = config::RTQ_DISK_PART_SIZE;
  // 初始化数据结构
  Data delete_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1); // 删除数据
  Data write_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);  // 写入数据
  Data read_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);   // 读取数据

  // 输入数据
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> delete_data[i][j];
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> write_data[i][j];
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> read_data[i][j]; // 读取数据
    }
  }
  // 初始化资源分配器并进行模拟退火优化
  auto [best_solution, alpha] = InitResourceAllocator(
      t, m, n, v, g, delete_data, write_data, read_data); // 获取最优解
  auto tsp = InitTSP(n, m, alpha, best_solution); // 初始化 TSP 问题

  // 初始化对象池、调度器、段管理器和磁盘管理器
  ObjectPool pool(t);
  Scheduler none(&pool, n, t, v);
  SegmentManager seg_mgr(m, n, v, best_solution, tsp);
  DiskManager dm(&pool, &none, &seg_mgr,alpha, n,m, v, g);
  TopScheduler tes(&timeslice, &none, &pool, &dm);

  // 同步函数
  auto sync = []() -> bool {
    std::string ts;
    int time;
    std::cin >> ts >> time;
    (std::cout << "TIMESTAMP " << timeslice << '\n').flush();
    return time == timeslice;
  };

  // 删除操作
  auto delete_op = [&]() -> void {
    int n_delete;
    std::cin >> n_delete;
    for (int i = 0; i < n_delete; i++) {
      int object_id;
      std::cin >> object_id;
      --object_id; // 转换为 0 索引
      tes.DeleteRequest(object_id);
    }
    printer::PrintDelete(); // 打印删除信息
  };

  // 写入操作
  auto write_op = [&]() -> void {
    int n_write;
    std::cin >> n_write;
    for (int i = 0; i < n_write; i++) {
      int id, size, tag; // NOLINT
      std::cin >> id >> size >> tag;
      --id;
      --tag;
      auto oid = tes.InsertRequest(id, size, tag); // 插入请求
      printer::AddInsertedObject(oid);             // 添加写入对象
    }
    printer::PrintWrite(pool); // 打印写入信息
  };

  // 读取操作
  auto read_op = [&]() -> void {
    int n_read;
    std::cin >> n_read;
    for (int i = 0; i < n_read; i++) {
      int request_id, object_id; // NOLINT
      std::cin >> request_id >> object_id;
      --object_id;                            // 转换为 0 索引
      tes.ReadRequest(request_id, object_id); // 读取请求
    }
  };

  (std::cout << "OK\n").flush(); // 输出初始化完成信息
  // 主循环，处理每个时间片
  for (timeslice = 1; timeslice <= t + 105; timeslice++) {
    sync();      // 同步时间片
    delete_op(); // 处理删除操作
    write_op();  // 处理写入操作
    read_op();   // 处理读取操作
    tes.Read();  // 执行读取操作

    if (timeslice % 1800 == 0){
      dm.GarbageCollection(k); // 垃圾回收
    }

#ifdef ISCERR
    if (timeslice == t + 105) {
      for (int i = 0; i < n; i++) {
        auto x = dm.GetDisk(i);
        std::cerr << "Disk " << i << " read count: " << x.GetReadCount()
                  << '\n';
      }
    }
#endif
    printer::PrintRead(n); // 打印读取信息
  }
}