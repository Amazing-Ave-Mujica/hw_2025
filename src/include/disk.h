#pragma once

#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice; // 全局变量，表示时间片
#endif

class Disk {
  friend class DiskManager; // 声明 DiskManager 为友元类，可以访问 Disk 的私有成员
  friend class Segment;     // 声明 Segment 为友元类，可以访问 Disk 的私有成员

public:
  // 表示空块的常量，用于标识未被占用的存储块
  static constexpr std::pair<int, int> EMPTY_BLOCK = {-1, -1};

  // 构造函数，初始化磁盘
  Disk(int disk_id, int V)
      : disk_id_(disk_id), capacity_(V), storage_(V, {-1, -1}) {
    // 初始化所有存储块为可用状态
    for (int i = 0; i < capacity_; i++) {
      free_block_idck_idck_.insert(i); // 将所有块的索引加入空闲块集合
    }
    prev_is_rd_ = false; // 初始化为非读取状态
    free_size_ = capacity_; // 空闲块数量等于总容量
  }

  // 写入数据到磁盘的空闲块
  auto Write(int oid, int y) -> int {
    auto it = free_block_idck_idck_.begin(); // 获取第一个空闲块
    assert(it != free_block_idck_idck_.end()); // 确保存在空闲块
    int idx = *it; // 获取空闲块的索引
    storage_[idx] = {oid, y}; // 将数据写入该块
    free_block_idck_idck_.erase(it); // 从空闲块集合中移除该块
    --free_size_; // 更新空闲块数量
    return idx; // 返回写入的块索引
  }

  // 写入数据到指定块或之后的第一个空闲块
  auto WriteBlock(int bid, int oid, int y) -> int {
    auto it = free_block_idck_idck_.lower_bound(bid); // 找到第一个大于等于 bid 的空闲块
    assert(it != free_block_idck_idck_.end()); // 确保存在空闲块
    int idx = *it; // 获取空闲块的索引
    storage_[idx] = {oid, y}; // 将数据写入该块
    free_block_idck_idck_.erase(it); // 从空闲块集合中移除该块
    --free_size_; // 更新空闲块数量
    return idx; // 返回写入的块索引
  }

  // 删除指定索引的块中的数据
  void Delete(int idx) {
    assert(idx >= 0 && idx < capacity_); // 确保索引合法
    if (free_block_idck_idck_.find(idx) != free_block_idck_idck_.end()) {
      return; // 如果块已经是空闲状态，则直接返回
    }
    storage_[idx] = {-1, -1}; // 将块重置为空块
    free_block_idck_idck_.insert(idx); // 将块重新加入空闲块集合
    ++free_size_; // 更新空闲块数量
  }

  // 计算读取操作的时间成本
  auto ReadCost() -> int {
    if (prev_is_rd_) { // 如果上一次操作是读取
      return std::max(16, (prev_rd_cost_ + 1) * 4 / 5); // 返回递减的读取成本
    }
    return 64; // 如果上一次操作不是读取，返回初始读取成本
  }

  // 执行读取操作
  void Read(int &time, int k = 1) {
    while (k-- > 0) {
      time -= (prev_rd_cost_ = ReadCost()); // 减去读取成本
      prev_is_rd_ = true; // 设置为读取状态
      itr_ = (itr_ + 1 == capacity_) ? 0 : itr_ + 1; // 更新迭代器位置
      assert(itr_ >= 0 && itr_ < capacity_); // 确保迭代器位置合法
      if (time < 0) {
        int x;
      }
      assert(time >= 0);
    }
  }

  // 跳转到指定块
  void Jump(int &time, int x) {
    assert(x >= 0 && x < capacity_); // 确保目标块索引合法
    prev_is_rd_ = false; // 设置为非读取状态
    time = 0; // 重置时间
    itr_ = x; // 更新迭代器位置
  }

  // 跳过当前块
  void Pass(int &time, int k = 1) {
    while (k-- > 0) {
      --time; // 减少时间
      prev_is_rd_ = false; // 设置为非读取状态
      itr_ = (itr_ + 1 == capacity_) ? 0 : itr_ + 1; // 更新迭代器位置
      assert(time >= 0);
    }
  }

  // 获取当前迭代器位置
  auto GetItr() -> int { return itr_; }

  // 获取指定索引的存储块内容
  auto GetStorageAt(int idx) -> std::pair<int, int> {
    assert(idx >= 0 && idx < capacity_); // 确保索引合法
    return storage_[idx]; // 返回存储块内容
  }

  // 获取从指定索引开始的最大连续空闲块长度
  auto GetMaxLen(int tag, int idx = 0, int len = INT32_MAX) {
    len = std::min(len, capacity_ - idx); // 确保长度不超过容量
    int res = 0; // 最大连续空闲块长度
    for (int i = idx, cnt = 0; i < len; i++) {
      cnt = (storage_[idx] == EMPTY_BLOCK) ? cnt + 1 : 0; // 计算连续空闲块
      res = std::max(res, cnt); // 更新最大长度
      if (cnt == 0) {
        break; // 如果遇到非空块，则停止计算
      }
    }
    return res; // 返回最大连续空闲块长度
  }

private:
  const int disk_id_; // 磁盘 ID
  const int capacity_; // 磁盘容量（块数）

  // 迭代器
  int itr_{}; // 当前迭代器位置
  int prev_rd_cost_; // 上一次读取操作的成本
  bool prev_is_rd_; // 上一次操作是否为读取

  int free_size_; // 当前空闲块数量
  std::set<int> free_block_idck_idck_; // 空闲块集合

  std::vector<std::pair<int, int>> storage_; // 存储块，存储每个块的内容（对象 ID 和数据）
};