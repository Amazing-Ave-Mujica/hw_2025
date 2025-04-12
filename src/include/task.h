#pragma once
#include <bits/stdc++.h>
// only read task
// 任务结构体，用于表示只读任务
struct Task {
  // 构造函数
  // 参数：
  // - tid: 任务的唯一标识符
  // - oid: 任务关联的对象 ID
  // - timestamp: 任务的时间戳
  Task(int tid, int oid, int timestamp, std::vector<std::pair<int, int>> &&work)
      : tid_(tid), oid_(oid), timestamp_(timestamp), work_(std::move(work)) {}

  // x -> y
  void TransWork(int x, int y) {
    for (auto &[a, b] : work_) {
      if (b == x) {
        b = y;
      }
    }
  }

  int tid_;                        // 任务的唯一标识符
  int oid_;                        // 任务关联的对象 ID
  [[maybe_unused]] int timestamp_; // 任务的时间戳，用于记录任务的创建时间
  [[maybe_unused]] int order_;     // 任务的顺序，用于调度时的优先级或排序
  std::vector<std::pair<int, int>>
      work_; // 记录对象的每个块是由哪个磁盘读, 在哪个块
};