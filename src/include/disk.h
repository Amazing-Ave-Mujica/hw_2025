#pragma once

#include <cassert>
#include <set>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice;
#endif

class Disk {
  friend class DiskManager;

public:
  Disk(int disk_id, int V) : disk_id_(disk_id), capacity_(V) {
    storage_.resize(capacity_);
    for (int i = 0; i < capacity_; i++) {
      free_block_.insert(i);
    }
    prev_is_rd_ = false;
    free_size_ = capacity_;
  }

  auto Write(int oid, int y) -> int {
    auto it = free_block_.begin();
    assert(it != free_block_.end());
    int idx = *it;
    storage_[idx] = {oid, y};
    free_block_.erase(it);
    --free_size_;
    return idx;
  }

  void Delete(int idx) {
    assert(idx >= 0 && idx < capacity_);
    if (free_block_.find(idx) != free_block_.end()) {
      return;
    }
    storage_[idx] = {-1, -1};
    free_block_.insert(idx);
    ++free_size_;
  }

  auto ReadCost() -> int {
    if (prev_is_rd_) {
      return std::max(16, (prev_rd_cost_ + 1) * 4 / 5);
    }
    return 64;
  }

  void Read(int &time) {
    time -= (prev_rd_cost_ = ReadCost());
    prev_is_rd_ = true;
    itr_ = (itr_ + 1 == capacity_) ? 0 : itr_ + 1;
    assert(itr_ >= 0 && itr_ < capacity_);
  }

  void Jump(int &time, int x) {
    assert(x >= 0 && x < capacity_);
    prev_is_rd_ = false;
    time = 0;
    itr_ = x;
  }

  void Pass(int &time) {
    assert(false);
    --time;
    prev_is_rd_ = false;
    itr_ = (itr_ + 1 == capacity_) ? 0 : itr_ + 1;
  }

  auto GetItr() -> int { return itr_; }

  auto GetStorageAt(int idx) -> std::pair<int, int> {
    assert(idx >= 0 && idx < capacity_);
    return storage_[idx];
  }

private:
  const int disk_id_;
  const int capacity_;

  // Iterator
  int itr_{};
  int prev_rd_cost_;
  bool prev_is_rd_;

  int free_size_;
  std::set<int> free_block_;

  std::vector<std::pair<int, int>> storage_;
};