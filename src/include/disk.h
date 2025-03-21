#pragma once

#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice;
#endif

class Disk {
  friend class DiskManager;
  friend class Segment;

public:
  static constexpr std::pair<int, int> EMPTY_BLOCK = {-1, -1};
  Disk(int disk_id, int V)
      : disk_id_(disk_id), capacity_(V), storage_(V, {-1, -1}) {
    for (int i = 0; i < capacity_; i++) {
      free_block_idck_idck_.insert(i);
    }
    prev_is_rd_ = false;
    free_size_ = capacity_;
  }

  auto Write(int oid, int y) -> int {
    auto it = free_block_idck_idck_.begin();
    assert(it != free_block_idck_idck_.end());
    int idx = *it;
    storage_[idx] = {oid, y};
    free_block_idck_idck_.erase(it);
    --free_size_;
    return idx;
  }

  auto WriteBlock(int bid, int oid, int y) -> int {
    auto it = free_block_idck_idck_.lower_bound(bid);
    assert(it != free_block_idck_idck_.end());
    int idx = *it;
    storage_[idx] = {oid, y};
    free_block_idck_idck_.erase(it);
    --free_size_;
    return idx;
  }

  void Delete(int idx) {
    assert(idx >= 0 && idx < capacity_);
    if (free_block_idck_idck_.find(idx) != free_block_idck_idck_.end()) {
      return;
    }
    storage_[idx] = {-1, -1};
    free_block_idck_idck_.insert(idx);
    ++free_size_;
  }

  // 64 51.2 40.9 32.7 26.2 20.9 16.7 13.4 10.7
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
    --time;
    prev_is_rd_ = false;
    itr_ = (itr_ + 1 == capacity_) ? 0 : itr_ + 1;
  }

  auto GetItr() -> int { return itr_; }

  auto GetStorageAt(int idx) -> std::pair<int, int> {
    assert(idx >= 0 && idx < capacity_);
    return storage_[idx];
  }

  auto GetMaxLen(int tag, int idx = 0, int len = INT32_MAX) {
    len = std::min(len, capacity_ - idx);
    int res = 0;
    for (int i = idx, cnt = 0; i < len; i++) {
      cnt = (storage_[idx] == EMPTY_BLOCK) ? cnt + 1 : 0;
      res = std::max(res, cnt);
      if (cnt == 0) {
        break;
      }
    }
    return res;
  }

private:
  const int disk_id_;
  const int capacity_;

  // Iterator
  int itr_{};
  int prev_rd_cost_;
  bool prev_is_rd_;

  int free_size_;
  std::set<int> free_block_idck_idck_;

  std::vector<std::pair<int, int>> storage_;
};