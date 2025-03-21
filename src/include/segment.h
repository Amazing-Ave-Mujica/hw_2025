#include "data.h"
#include <algorithm>
#include <cstdint>
#include <list>
#include <vector>
class Segment {
public:
  static constexpr int DEFAULT_CAPACITY = 10;
  int disk_id_, disk_addr_, tag_, capacity_, size_{0};
  Segment(int disk_id, int disk_addr, int tag, int capacity = DEFAULT_CAPACITY)
      : disk_id_(disk_id), disk_addr_(disk_addr), tag_(tag),
        capacity_(capacity) {}
  void Expand(int len) { capacity_ += std::min(len, capacity_); }
  void Write(int size) { size_ += size; }
  void Delete(int size) { size_ -= size; }
};

class SegmentManager {
public:
  std::vector<std::list<Segment>> segs_;
  using data_t = std::vector<std::vector<int>>;
  SegmentManager(int M, int N, int V, const data_t &t) : segs_(M) {

    for (int i = 0; i < N; i++) {
      int addr = 0;
      int rem = V;
      for (int j = 0; j < M; j++) {
        auto cur = std::min(t[j][i], rem);
        segs_[j].emplace_back(i, addr, j, cur);
        rem -= cur;
        addr += cur;
      }
    }
  }
  auto Find(int tag, int disk_id, int size) -> Segment * {
    for (auto &s : segs_[tag]) {
      if (s.disk_id_ != disk_id) {
        continue;
      }
      if (s.size_ + size <= s.capacity_) {
        return &s;
      }
    }
    return nullptr;
  }
  auto FindBlock(int tag, int disk_id, int block_id) -> Segment * {
    for (auto &s : segs_[tag]) {
      if (s.disk_id_ != disk_id) {
        continue;
      }
      if (s.disk_addr_ <= block_id && block_id < s.disk_addr_ + s.capacity_) {
        return &s;
      }
    }
    return nullptr;
  }
  auto Delete(int tag, int disk_id, int block_id) {
    for (auto &s : segs_[tag]) {
      if (s.disk_id_ != disk_id) {
        continue;
      }
      if (s.disk_addr_ <= block_id && block_id <= s.disk_addr_ + s.capacity_) {
        s.Delete(1);
        return true;
      }
    }
    return false;
  }
};