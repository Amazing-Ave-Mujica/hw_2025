#pragma once

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice; // 全局变量，表示时间片
#endif

// 对象结构体，用于表示存储系统中的对象
struct Object {
  // 构造函数
  // 参数：
  // - id: 对象的唯一标识符
  // - tag: 对象的标签，用于分类
  // - size: 对象的大小（块数）
  Object(int id, int tag, int size) : id_(id), tag_(tag), size_(size) {
    for (auto &i : tdisk_) {
      i.resize(size_, {}); // 初始化每个副本的块信息
    }
    valid_ = true; // 对象初始状态为有效
  }

  bool valid_;                // 对象是否有效
  int id_;                    // 对象的唯一标识符
  int tag_;                   // 对象的标签
  int size_;                  // 对象的大小（块数）
  std::array<int, 3> idisk_;  // 存储对象的副本所在的磁盘 ID（最多 3 个副本）
  std::vector<int> tdisk_[3]; // 每个副本的块信息（块 ID 列表）
};

// 对象池类，用于管理对象的创建和访问
class ObjectPool {
public:
  // 构造函数
  // 参数：
  // - T: 对象池的初始容量
  explicit ObjectPool(int T) { objs_.reserve(T + 105); } // 预留空间以提高性能

  // 创建新对象
  // 参数：
  // - id: 对象的唯一标识符
  // - tag: 对象的标签
  // - size: 对象的大小（块数）
  // 返回值：新对象的索引
  auto NewObject(int id, int tag, int size) -> int {
    objs_.emplace_back(
        std::make_shared<Object>(id, tag, size)); // 创建新对象并存储
    return size_++;                               // 返回新对象的索引
  }

  // 获取指定索引的对象
  // 参数：
  // - oid: 对象的索引
  // 返回值：共享指针，指向对象
  auto GetObjAt(int oid) -> std::shared_ptr<Object> {
    assert(oid >= 0 && oid < size_); // 确保索引合法
    return objs_[oid];
  }

  // 检查对象是否有效
  // 参数：
  // - oid: 对象的索引
  // 返回值：布尔值，表示对象是否有效
  auto IsValid(int oid) -> bool { return objs_[oid]->valid_; }

  // 将对象标记为无效
  // 参数：
  // - oid: 对象的索引
  void Drop(int oid) {
    objs_[oid]->valid_ = false;
    // objs_[oid].reset();
  }

  

private:
  int size_{};                                // 当前对象池中的对象数量
  std::vector<std::shared_ptr<Object>> objs_; // 存储对象的共享指针列表
};