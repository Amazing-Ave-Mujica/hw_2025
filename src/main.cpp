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

constexpr int TIME_SLICE_DIVISOR = 1800; // 常量替代魔法数字

int timeslice = 0;

auto main() -> int {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  int t, m, n, v, g; // NOLINT
  std::cin >> t >> m >> n >> v >> g;

  Data delete_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);
  Data write_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);
  Data read_data(m, ((t - 1) / TIME_SLICE_DIVISOR) + 1);

  std::vector<std::vector<int>> timeslice_data(
      m, std::vector<int>(((t - 1) / TIME_SLICE_DIVISOR) + 1, 0));
  std::vector<std::vector<double>> alpha(
      m, std::vector<double>(m, 0.0)); // 资源混合惩罚系数
  std::vector<double> beta(m, 1);    // 超参数*1
  std::vector<int> max_allocate(m, 0), resource(m, 0); // NOLINT

  // 输入数据并初始化 timeslice_data
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> delete_data[i][j];
      timeslice_data[i][j] -= delete_data[i][j];
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> write_data[i][j];
      timeslice_data[i][j] += write_data[i][j];
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      std::cin >> read_data[i][j];
    }
  }

  // 计算 timeslice_data 的累积和和 max_allocate
  for (int i = 0; i < m; i++) {
    for (int j = 1; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      timeslice_data[i][j] += timeslice_data[i][j - 1];
    }
    max_allocate[i] =
        *std::max_element(timeslice_data[i].begin(), timeslice_data[i].end());
  }
  for (int i = 0; i < m; i++) {
    if (i < m - 1) {
      resource[i] = static_cast<int>(
          1.0 * max_allocate[i] * (n * v) /
          std::accumulate(max_allocate.begin(), max_allocate.end(), 0));//调参*2
      // resource[i]=n*v/m;
    } else {
      resource[i] =
          n * v - std::accumulate(resource.begin(), resource.end(), 0);
    }
  }

  // 计算 alpha 矩阵
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < m; j++) {
      int sum = 0;
      for (int k = 0; k < (t - 1) / TIME_SLICE_DIVISOR + 1; k++) {
        alpha[i][j] += std::min(timeslice_data[i][k], timeslice_data[j][k]);
        sum += std::max(timeslice_data[i][k], timeslice_data[j][k]);
      }
      if (sum > 0) { // 避免除以零
        alpha[i][j] /= sum;
      }
    }
  }

  (std::cout << "OK\n").flush();
  // std::cerr<<v/m<<'\n';
  ResourceAllocator ra(m, n, v,  v / m, resource, alpha, beta);//调参*3
  ra.SimulatedAnnealing(2000000000, 0.9995,100000000);//调参*4
  auto best_solution = ra.GetBestSolution();
  ObjectPool pool(t);
  Scheduler none(&pool, n, t);
  SegmentManager seg_mgr(m, n, v, best_solution);
  DiskManager dm(&pool, &none, &seg_mgr, n, v, g);
  TopScheduler tes(&timeslice, &none, &pool, &dm);
  auto sync = []() -> bool {
    std::string ts;
    int time;
    std::cin >> ts >> time;
    (std::cout << "TIMESTAMP " << timeslice << '\n').flush();
    return time == timeslice;
  };

  auto delete_op = [&]() -> void {
    int n_delete;
    std::cin >> n_delete;
    for (int i = 0; i < n_delete; i++) {
      int object_id;
      std::cin >> object_id;
      --object_id;
      tes.DeleteRequest(object_id);
    }
    printer::PrintDelete();
  };

  auto write_op = [&]() -> void {
    int n_write;
    std::cin >> n_write;
    for (int i = 0; i < n_write; i++) {
      int id, size, tag; // NOLINT
      std::cin >> id >> size >> tag;
      --id;
      --tag;
      auto oid = tes.InsertRequest(id, size, tag);
      printer::AddInsertedObject(oid);
    }
    printer::PrintWrite(pool);
  };

  auto read_op = [&]() -> void {
    int n_read;
    std::cin >> n_read;
    for (int i = 0; i < n_read; i++) {
      int request_id, object_id; // NOLINT
      std::cin >> request_id >> object_id;
      --object_id;
      tes.ReadRequest(request_id, object_id);
    }
  };

  for (timeslice = 1; timeslice <= t + 105; timeslice++) {
    sync();
    delete_op();
    write_op();
    read_op();
    tes.Read();
    printer::PrintRead(n);
  }
}