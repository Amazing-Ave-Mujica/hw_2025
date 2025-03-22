#include "include/config.h"
#include "include/data.h"
#include "include/disk.h"
#include "include/disk_manager.h"
#include "include/object.h"
#include "include/printer.h"
#include "include/scheduler.h"
#include "include/top_scheduler.h"
#include "resource_allocator.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <numeric>

constexpr int TIME_SLICE_DIVISOR = config::TIME_SLICE_DIVISOR; // 常量替代魔法数字

int timeslice = 0;

auto main() -> int {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  int t, m, n, v, g; // NOLINT
  std::cin >> t >> m >> n >> v >> g; // 输入时间片数量、标签数量、磁盘数量、磁盘容量和生命周期

  // 初始化数据结构
  Data delete_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1); // 删除数据
  Data write_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);  // 写入数据
  Data read_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);   // 读取数据

  std::vector<std::vector<int>> timeslice_data(
      m, std::vector<int>(((t - 1) / TIME_SLICE_DIVISOR) + 1, 0)); // 时间片数据
  std::vector<std::vector<double>> alpha(
      m, std::vector<double>(m, 0.0)); // 资源混合惩罚系数
  std::vector<double> beta(m, 1);    // 超参数*1
  std::vector<int> max_allocate(m, 0), resource(m, 0); // NOLINT

  // 输入数据并初始化 timeslice_data
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> delete_data[i][j];
      timeslice_data[i][j] -= delete_data[i][j]; // 减去删除数据
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> write_data[i][j];
      timeslice_data[i][j] += write_data[i][j]; // 加上写入数据
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> read_data[i][j]; // 读取数据
    }
  }

  // 计算 timeslice_data 的累积和和 max_allocate
  for (int i = 0; i < m; i++) {
    for (int j = 1; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      timeslice_data[i][j] += timeslice_data[i][j - 1]; // 累积和
    }
    max_allocate[i] =
        *std::max_element(timeslice_data[i].begin(), timeslice_data[i].end()); // 最大分配值
  }

  // 计算资源分配
  for (int i = 0; i < m; i++) {
    if (i < m - 1) {
      resource[i] = static_cast<int>(
          1.0 * max_allocate[i] * (n * v) /
          std::accumulate(max_allocate.begin(), max_allocate.end(), 0)); // 按比例分配资源
    } else {
      resource[i] =
          n * v - std::accumulate(resource.begin(), resource.end(), 0); // 剩余资源分配给最后一个标签
    }
  }

  // 计算 alpha 矩阵
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < m; j++) {
      int sumi = 0, sumj = 0; // NOLINT
      for (int k = 0; k < (t - 1) / TIME_SLICE_DIVISOR + 1; k++) {
        alpha[i][j] += std::min(timeslice_data[i][k], timeslice_data[j][k]); // 计算混合惩罚项
        sumi += timeslice_data[i][k];
        sumj += timeslice_data[j][k];
      }
      int sum = std::min(sumi, sumj);
      if (sum > 0) { // 避免除以零
        alpha[i][j] /= sum;
      }
    }
  }

  (std::cout << "OK\n").flush(); // 输出初始化完成信息

  // 初始化资源分配器并进行模拟退火优化
  ResourceAllocator ra(m, n, v, v/m, resource, alpha, beta); // 调参*3
  ra.SimulatedAnnealing(config::T, config::COOLING_RATE, config::MAX_ITER); // 调参*4
  auto best_solution = ra.GetBestSolution(); // 获取最优解

  // 初始化对象池、调度器、段管理器和磁盘管理器
  ObjectPool pool(t);
  Scheduler none(&pool, n, t);
  SegmentManager seg_mgr(m, n, v, best_solution);
  DiskManager dm(&pool, &none, &seg_mgr, n, v, g);
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
      printer::AddInsertedObject(oid); // 添加写入对象
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
      --object_id; // 转换为 0 索引
      tes.ReadRequest(request_id, object_id); // 读取请求
    }
  };

  // 主循环，处理每个时间片
  for (timeslice = 1; timeslice <= t + 105; timeslice++) {
    sync();       // 同步时间片
    delete_op();  // 处理删除操作
    write_op();   // 处理写入操作
    read_op();    // 处理读取操作
    tes.Read();   // 执行读取操作
    printer::PrintRead(n); // 打印读取信息
  }
}