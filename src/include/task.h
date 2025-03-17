#include <vector>

// only read task
struct Task {
  Task(int tid, int oid, int block_cnt, int timestamp) : tid_(tid), oid_(oid), timestamp_(timestamp) {
    valid_ = true;
    rd_.resize(block_cnt, false);
  }

  bool valid_;
  int tid_;
  int timestamp_;
  int oid_;
  std::vector<bool> rd_;
  [[maybe_unused]] int order_;
};