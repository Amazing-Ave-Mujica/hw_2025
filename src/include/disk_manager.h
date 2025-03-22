#pragma once

#include "disk.h"
#include "object.h"
#include "printer.h"
#include "scheduler.h"
#include "segment.h"
#include <algorithm>
#include <cassert>
#include <random>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice; // 全局变量，表示时间片
#endif

class DiskManager {
public:
  // 构造函数，初始化 DiskManager
  // 参数：
  // - obj_pool: 对象池，用于管理对象
  // - scheduler: 调度器，用于管理磁盘调度
  // - seg_mgr: 段管理器，用于管理磁盘段
  // - N: 磁盘数量
  // - V: 每个磁盘的容量
  // - G: 磁盘的生命周期
  DiskManager(ObjectPool *obj_pool, Scheduler *scheduler,
              SegmentManager *seg_mgr, int N, int V, int G)
      : disk_cnt_(N), life_(G), obj_pool_(obj_pool), scheduler_(scheduler),
        seg_mgr_(seg_mgr) {
    disks_.reserve(disk_cnt_); // 预留磁盘数量的空间
    for (int i = 0; i < disk_cnt_; i++) {
      disks_.emplace_back(i, V); // 初始化每个磁盘
    }
  }

  // 获取磁盘数量
  auto GetDiskCnt() const -> int { return disk_cnt_; }

  // 插入对象的第 kth 个副本到磁盘
  // 参数：
  // - oid: 对象 ID
  // - kth: 副本编号
  // 返回值：是否插入成功
  auto Insert(int oid, int kth) -> bool {
    auto object = obj_pool_->GetObjAt(oid); // 获取对象

    // 随机打乱磁盘顺序
    std::vector<int> sf(disks_.size());
    std::iota(sf.begin(), sf.end(), 0);
    std::mt19937 rng(0);
    std::shuffle(sf.begin(), sf.end(), rng);

    // 按块写入数据
    auto write_by_block = [&]() {
      for (auto od : sf) {
        auto &disk = disks_[od];
        if (disk.free_size_ >= object->size_) { // 检查磁盘是否有足够空间
          bool exist = false;
          for (int i = 0; i < kth; i++) {
            exist |= (object->idisk_[i] == disk.disk_id_); // 检查是否已存在副本
          }
          if (!exist) {
            object->idisk_[kth] = disk.disk_id_; // 设置副本所在磁盘
            for (int j = 0; j < object->size_; j++) {
              auto block_id = disk.Write(oid, j); // 写入数据到磁盘
              object->tdisk_[kth][j] = block_id; // 记录块 ID
              for (int i = 0, len = seg_mgr_->segs_.size(); i < len; i++) {
                auto ptr = seg_mgr_->FindBlock(i, od, block_id);
                if (ptr != nullptr) {
                  ptr->Write(1); // 更新段信息
                }
              }
            }
            return true; // 写入成功
          }
        }
      }
      return false; // 写入失败
    };

    // 按段写入数据
    auto write_by_segment = [&]() {
      int tag = object->tag_; // 获取对象的标签
      for (auto od : sf) {
        auto &disk = disks_[od];
        auto ptr = seg_mgr_->Find(tag, od, object->size_); // 查找合适的段
        if (ptr == nullptr) {
          continue; // 如果没有找到合适的段，跳过
        }
        bool exist = false;
        for (int i = 0; i < kth; i++) {
          exist |= (object->idisk_[i] == disk.disk_id_); // 检查是否已存在副本
        }
        if (exist) {
          continue; // 如果已存在副本，跳过
        }
        object->idisk_[kth] = od; // 设置副本所在磁盘
        for (int j = 0; j < object->size_; j++) {
          object->tdisk_[kth][j] = disk.WriteBlock(ptr->disk_addr_, oid, j); // 写入数据到段
        }
        ptr->Write(object->size_); // 更新段信息
        return true; // 写入成功
      }
      return false; // 写入失败
    };

    // 优先按段写入，如果失败则按块写入
    if (write_by_segment()) {
      return true;
    }
    if (write_by_block()) {
      return true;
    }
    return false; // 写入失败
  }

  // 删除指定块的数据
  // 参数：
  // - tag: 数据标签
  // - disk_id: 磁盘 ID
  // - block_id: 块 ID
  void Delete(int tag, int disk_id, int block_id) {
    seg_mgr_->Delete(tag, disk_id, block_id); // 删除段信息
    disks_[disk_id].Delete(block_id); // 删除磁盘块数据
  }

  // 从指定磁盘读取数据
  // 参数：
  // - disk_id: 磁盘 ID
  void Read(int disk_id) {
    int time = life_; // 初始化读取时间
    auto &disk = disks_[disk_id];
    while (time > 0) {
      const auto x = scheduler_->GetRT(disk_id, disk.itr_); // 获取调度器推荐的读取位置
      if (x == -1) {
        break; // 如果没有推荐位置，退出
      }
      if (disk.itr_ == x) {
        if (disk.ReadCost() <= time) { // 如果有足够时间完成读取
          disk.Read(time); // 执行读取操作
          printer::ReadAddRead(disk_id, 1); // 打印读取信息
          assert(x >= 0 && x < disk.capacity_);
          auto [oid, y] = disk.GetStorageAt(x); // 获取读取的数据
          scheduler_->Update(oid, y); // 更新调度器信息
          continue;
        }
        break; // 如果时间不足，退出
      }
      if (ReadDist(disk_id, x) >= life_ && time == life_) {
        disk.Jump(time, x); // 跳转到目标位置
        printer::ReadSetJump(disk_id, x + 1); // 打印跳转信息
      } else {
        int rd_cost = disk.ReadCost();
        if (time >= rd_cost && ReadDist(disk_id, x) <= 12 && rd_cost <= 17) {
          auto [oid, y] = disk.GetStorageAt(disk.itr_);
          disk.Read(time); // 执行读取操作
          printer::ReadAddRead(disk_id, 1); // 打印读取信息
          if (oid >= 0) {
            scheduler_->Update(oid, y); // 更新调度器信息
          }
        } else {
          disk.Pass(time); // 跳过当前块
          printer::ReadAddPass(disk_id, 1); // 打印跳过信息
        }
      }
    }
  }

  // 计算从当前位置到目标位置的距离
  // 参数：
  // - disk_id: 磁盘 ID
  // - dest: 目标位置
  // 返回值：距离
  auto ReadDist(int disk_id, int dest) -> int {
    auto &disk = disks_[disk_id];
    int siz = disk.capacity_;
    int pos = disk.itr_;
    return (dest - pos + siz) % siz; // 计算环形磁盘上的距离
  }

  // 获取磁盘的压力（读取距离）
  // 参数：
  // - disk_id: 磁盘 ID
  // - dest: 目标位置
  // 返回值：压力值
  auto GetStress(int disk_id, int dest) -> int {
    return ReadDist(disk_id, dest);
  }

private:
  const int disk_cnt_; // 磁盘数量
  const int life_; // 磁盘生命周期
  ObjectPool *obj_pool_; // 对象池
  Scheduler *scheduler_; // 调度器
  SegmentManager *seg_mgr_; // 段管理器
  std::vector<Disk> disks_; // 磁盘列表
};