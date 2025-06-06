#include "config.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <list>
#include <random>
#include <vector>

class Segment {
public:
  static constexpr int DEFAULT_CAPACITY =
      config::SEGMENT_DEFAULT_CAPACITY; // 默认段容量

  int disk_id_;   // 段所在的磁盘 ID
  int disk_addr_; // 段的起始地址（块编号）
  int tag_;       // 段的标签，用于分类
  int capacity_;  // 段的总容量（块数）
  int size_{0};   // 段当前已使用的大小（块数）

  // 构造函数
  // 参数：
  // - disk_id: 段所在的磁盘 ID
  // - disk_addr: 段的起始地址
  // - tag: 段的标签
  // - capacity: 段的总容量（默认值为 DEFAULT_CAPACITY）
  Segment(int disk_id, int disk_addr, int tag, int capacity = DEFAULT_CAPACITY)
      : disk_id_(disk_id), disk_addr_(disk_addr), tag_(tag),
        capacity_(capacity) {}

  // 扩展段的容量
  // 参数：
  // - len: 要扩展的长度
  void Expand(int len) { capacity_ += std::min(len, capacity_); }

  // 写入数据到段
  // 参数：
  // - size: 写入的块数
  void Write(int size) { size_ += size; }

  // 从段中删除数据
  // 参数：
  // - size: 删除的块数
  void Delete(int size) {
    assert(size_ >= size);
    size_ -= size;
  }
};

class SegmentManager {
public:
  std::vector<std::list<Segment>> segs_; // 每个标签对应的段列表
  std::vector<int> seg_disk_capacity_, seg_disk_size_;

  using data_t = std::vector<std::vector<int>>; // 数据类型，用于初始化段

  // 构造函数
  // 参数：
  // - M: 标签数量
  // - N: 磁盘数量
  // - V: 每个磁盘的容量
  // - t: 每个标签在每个磁盘上的初始分配
  SegmentManager(int M, int N, int V, const data_t &t,
                 const std::vector<std::vector<int>> &tsp)
      : segs_(M), seg_disk_capacity_(N), seg_disk_size_(N) {
    for (int i = 0; i < N; i++) {   // 遍历每个磁盘
      int addr = 0;                 // 当前磁盘的起始地址
      int rem = V;                  // 当前磁盘的剩余容量
      for (int _ = 0; _ < M; _++) { // 遍历每个标签
        int j = tsp[i][_];
        auto cur = std::min(t[j][i], rem); // 当前标签在该磁盘上的分配
        segs_[j].emplace_back(i, addr, j,
                              cur); // 创建段并添加到对应标签的段列表
        rem -= cur;                 // 更新剩余容量
        addr += cur;                // 更新起始地址
      }
      for (int _ = 0; _ < M; _++) { // 遍历每个标签
        int j = tsp[i + N][_];
        auto cur = std::min(t[j][i + N], rem); // 当前标签在该磁盘上的分配
        segs_[j].emplace_back(i, addr, j,
                              cur); // 创建段并添加到对应标签的段列表
        rem -= cur;                 // 更新剩余容量
        addr += cur;                // 更新起始地址
      }
      seg_disk_capacity_[i] = addr;
    }
  }

  // 查找满足条件的段
  // 参数：
  // - tag: 段的标签
  // - disk_id: 磁盘 ID
  // - size: 需要的块数
  // 返回值：指向满足条件的段的指针，如果没有找到则返回 nullptr
  auto Find(int tag, int disk_id, int size) -> Segment * {
    static std::mt19937 rng(config::RANDOM_SEED);
    std::array<Segment *, 3> vec;
    int tot = 0;
    for (auto &s : segs_[tag]) {   // 遍历指定标签的段列表
      if (s.disk_id_ != disk_id) { // 如果段不在指定磁盘上，跳过
        continue;
      }
      if (s.size_ + size <= s.capacity_) { // 如果段有足够的剩余容量
        vec[tot++] = &s;                   // 返回段的指针
      }
    }
    if (tot == 0) {
      return nullptr; // 没有找到满足条件的段
    }
    return vec[rng() % tot];
  }

  // 查找包含指定块的段
  // 参数：
  // - tag: 段的标签
  // - disk_id: 磁盘 ID
  // - block_id: 块 ID
  // 返回值：指向包含指定块的段的指针，如果没有找到则返回 nullptr
  auto FindBlock(int tag, int disk_id, int block_id) -> Segment * {
    for (auto &s : segs_[tag]) {   // 遍历指定标签的段列表
      if (s.disk_id_ != disk_id) { // 如果段不在指定磁盘上，跳过
        continue;
      }
      if (s.disk_addr_ <= block_id && block_id < s.disk_addr_ + s.capacity_) {
        return &s;
      }
    }
    return nullptr;
  }

  auto FreeBlockSize(int idx) {
    return seg_disk_capacity_[idx] - seg_disk_size_[idx];
  }

  auto Write(Segment *ptr, int size) {
    seg_disk_size_[ptr->disk_id_] += size;
    ptr->size_ += size;
  }
  // 删除指定块所在的段中的数据
  // 参数：
  // - tag: 段的标签
  // - disk_id: 磁盘 ID
  // - block_id: 块 ID
  // 返回值：布尔值，表示是否成功删除
  auto Delete(int tag, int disk_id, int block_id) {
    for (auto &s : segs_[tag]) {   // 遍历指定标签的段列表
      if (s.disk_id_ != disk_id) { // 如果段不在指定磁盘上，跳过
        continue;
      }
      if (s.disk_addr_ <= block_id && block_id < s.disk_addr_ + s.capacity_) {
        // 如果块在段的范围内
        seg_disk_size_[disk_id]--;
        s.Delete(1); // 从段中删除一个块
        return true; // 删除成功
      }
    }
    return false; // 没有找到包含指定块的段
  }
  auto Trans(int disk_id, int x, int y) -> void {
    for (auto &lst : segs_) {
      for (auto &s : lst) {          // 遍历所有标签的段列表
        if (s.disk_id_ != disk_id) { // 如果段不在指定磁盘上，跳过
          continue;
        }
        if (s.disk_addr_ <= x && x < s.disk_addr_ + s.capacity_) {
          s.Delete(1); // 从段中删除一个块
        }
        if (s.disk_addr_ <= y && y < s.disk_addr_ + s.capacity_) {
          s.Write(1); // 从段中删除一个块
        }
      }
    }
  }
};