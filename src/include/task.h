#pragma once

#include <vector>

// only read task
struct Task {
  Task(int tid, int oid, int block_cnt, int timestamp)
      : tid_(tid), oid_(oid), timestamp_(timestamp) {}
  int tid_;
  int oid_;
  [[maybe_unused]] int timestamp_;
  [[maybe_unused]] int order_;
};