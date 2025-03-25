#pragma once

#include "config.h"
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
    static std::mt19937 rng(config::RANDOM_SEED);
    auto object = obj_pool_->GetObjAt(oid); // 获取对象

    // 随机打乱磁盘顺序
    std::vector<int> sf(disks_.size());
    std::iota(sf.begin(), sf.end(), 0);
    std::shuffle(sf.begin(), sf.end(), rng);

    // 按块写入数据
    auto write_by_block = [&]() {
      for (auto od : sf) {
        auto &disk = disks_[od];

        if constexpr (config::WritePolicy() == config::compact) {
          if (disk.free_size_ - seg_mgr_->FreeBlockSize(od) < object->size_) {
            continue;
          }
        } else if constexpr (config::WritePolicy() == config::none) {
          if (disk.free_size_ < object->size_) {
            continue;
          }
        } else {
          assert(false);
        }
        bool exist = false;
        for (int i = 0; i < kth; i++) {
          exist |= (object->idisk_[i] == disk.disk_id_); // 检查是否已存在副本
        }
        if (!exist) {
          object->idisk_[kth] = disk.disk_id_; // 设置副本所在磁盘
          for (int j = 0; j < object->size_; j++) {
            if (config::WritePolicy() == config::compact) {
              auto block_id = disk.WriteBlock(seg_mgr_->seg_disk_capacity_[od],
                                              oid, j); // 写入数据到磁盘
              object->tdisk_[kth][j] = block_id;       // 记录块 ID
            } else {
              auto block_id = disk.WriteBlock(0, oid, j); // 写入数据到磁盘
              object->tdisk_[kth][j] = block_id;          // 记录块 ID
              for (int i = 0, len = seg_mgr_->segs_.size(); i < len; i++) {
                auto ptr = seg_mgr_->FindBlock(i, od, block_id);
                if (ptr != nullptr) {
                  assert(false);
                  seg_mgr_->Write(ptr, 1); // 更新段信息
                }
              }
            }
          }
          return true; // 写入成功
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
          object->tdisk_[kth][j] =
              disk.WriteBlock(ptr->disk_addr_, oid, j); // 写入数据到段
        }
        seg_mgr_->Write(ptr, object->size_); // 更新段信息
        return true;                         // 写入成功
      }
      return false; // 写入失败
    };
    // 从段内写入一整块
    auto write_by_segment_whole = [&]() {
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
        auto [pos, maxlen] =
            disk.GetMaxLen(ptr->disk_addr_, ptr->disk_addr_ + ptr->capacity_);
        if (maxlen < object->size_) {
          continue;
        }
        object->idisk_[kth] = od; // 设置副本所在磁盘
        for (int j = 0; j < object->size_; j++) {
          object->tdisk_[kth][j] = disk.WriteBlock(pos, oid, j); // 写入数据到段
        }
        seg_mgr_->Write(ptr, object->size_); // 更新段信息
        return true;                         // 写入成功
      }
      return false; // 写入失败
    };

    // 优先按段写入，如果失败则按块写入
    if (kth == 0 && write_by_segment_whole()) {
      return true;
    }
    if (kth == 0 && write_by_segment()) {
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
    disks_[disk_id].Delete(block_id);         // 删除磁盘块数据
  }

  // 从指定磁盘读取数据
  // 参数：
  // - disk_id: 磁盘 ID
  void ReadSingle(int disk_id, int time) {
    auto &disk = disks_[disk_id];
    while (time > 0) {
      const auto x =
          scheduler_->GetRT(disk_id, disk.itr_); // 获取调度器推荐的读取位置
      if (x == -1) {
        break; // 如果没有推荐位置，退出
      }
      if (disk.itr_ == x) {
        break;
      }
      int rd_cost = disk.ReadCost();
      if (ReadDist(disk_id, x) <= 12) {
        if (time >= rd_cost) {
          auto [oid, y] = disk.GetStorageAt(disk.itr_);
          disk.Read(time);                  // 执行读取操作
          printer::ReadAddRead(disk_id, 1); // 打印读取信息
          if (oid >= 0) {
            scheduler_->Update(oid, y); // 更新调度器信息
          }
        } else if (rd_cost >= 64 && time > 0) {
          disk.Pass(time);                  // 跳过当前块
          printer::ReadAddPass(disk_id, 1); // 打印跳过信息
        } else {
          break;
        }
      } else {
        disk.Pass(time);                  // 跳过当前块
        printer::ReadAddPass(disk_id, 1); // 打印跳过信息
      }
    }
  }
  void Read(int disk_id) {
    int time = life_; // 初始化读取时间

#ifdef SINGLE_READ_MODE
    ReadSingle(disk_id, time);
#else
    auto &disk = disks_[disk_id];

    // 读取最近的 k 个任务，按距离升序存储
    auto task_k =
        scheduler_->GetRTK(disk_id, disk.GetItr(), config::DISK_READ_FETCH_LEN);

    if (task_k.empty()) {
      return;
    }
    auto [target, hot_cnt] = scheduler_->GetHotRT(disk_id);
    // 如果最近的任务都太远，就直接 jump
    if (ReadDist(disk_id, task_k[0]) >= life_ ||
        ReadDist(disk_id, task_k[0]) >= config::DISK_READ_FETCH_LEN / 3 ||
        hot_cnt - scheduler_->GetCntRT(disk_id, disk.itr_) >=
                config::JUMP_THRESHOLD &&
            ReadDist(disk_id, task_k[0]) >= config::DISK_READ_FETCH_LEN / 10) {
      disk.Jump(time, target);               // 跳转到目标位置
      printer::ReadSetJump(disk_id, target); // 打印跳转信息
      return;
    }

    /* 动态规划读取方案
     * dp[i][j] 表示完成了前 i 个任务，读最后一个任务花费 val[j]
     所用的最小总费用
     * val = {16, 19, 23, 28, 34, 42, 52, 64, 0x3f3f3f3f}
     * 如果不可能完成置为 -1
     * fr[i][j] 表示该状态从哪里转移过来
      {cost, op} 表示从 dp[i - 1][cost] 转移过来，op 表示中间的空白块是 pass /
     read
    */

    int task_cnt = task_k.size();
    // val.size() = 9
    static const std::vector<int> val = {16, 19, 23, 28,       34,
                                         42, 52, 64, life_ + 1};
    // +1 是因为会有一个伪装任务
    std::vector<std::vector<int>> dp(task_cnt + 1,
                                     std::vector<int>(val.size(), life_ + 1));
    std::vector<std::vector<std::pair<int, int>>> fr(
        task_cnt + 1, std::vector<std::pair<int, int>>(val.size(), {-1, -1}));

    /**
     * @brief 计算穿过连续空白块的花费
     * @param val_id 表示最近的读的花费为 val[x]
     * @param len 表示空白块的长度
     * @param op 表示 pass / read
     */
    static auto cross_empty_blank_cost = [](int val_id, int len,
                                            int op) -> int {
      if (len <= 0) {
        return 0;
      }
      if (op == 0) {
        return len;
      }
      int sum = 0;
      while (val_id > 0 && len > 0) {
        sum += val[--val_id];
        --len;
      }
      sum += len * val[0];
      return sum;
    };

    // 磁盘上 x 块走到 y 块的距离
    auto two_block_dist = [disk](int x, int y) -> int {
      int siz = disk.capacity_;
      return (y - x + siz) % siz;
    };

    // 单独处理第 1 个任务
    // 先确定上一个时间片指针最终的状态
    {
      int val_id = static_cast<int>(val.size()) - 1;
      if (disk.prev_is_rd_) {
        for (auto i = 0; i < val.size() - 1; i++) {
          if (disk.prev_rd_cost_ == val[i]) {
            val_id = i;
            break;
          }
        }
      }

      // 在队列头添加一个伪任务
      task_k.insert(
          task_k.begin(),
          disk.GetIterPre()); //@ttao: 这里是不是应该是 disk.GetItr() -1
      dp[0][val_id] = 0;
    }

    auto update_dp = [&](int i, int val_id, int pre_val_id, int len, int op) {
      // 跳过空白块并且读了当前块
      int res = dp[i - 1][pre_val_id] +
                cross_empty_blank_cost(pre_val_id, len, op) + val[val_id];
      if (dp[i][val_id] > res && res <= life_) {
        dp[i][val_id] = res;
        fr[i][val_id] = {pre_val_id, op};
      }
    };

    for (int i = 1; i <= task_cnt; i++) {
      for (int j = 0; j < val.size(); j++) {
        if (dp[i - 1][j] <= life_) {
          int len = two_block_dist(task_k[i - 1], task_k[i]) - 1;
          if (len > 0) {
            // read 空白块
            update_dp(i, std::max(0, j - len - 1), j, len, 1);
            // pass 空白块
            update_dp(i, val.size() - 2, j, len, 0);
          } else {
            update_dp(i, std::max(0, j - 1), j, 0, 0);
          }
        }
      }
    }

    // 导出方案
    // 先找找最远能读到哪个，然后往回构造方案
    std::pair<int, int> state = {0, 0};
    for (int i = task_cnt; i >= 1 && state.first == 0; i--) {
      for (int j = 0; j < val.size(); j++) {
        if (dp[i][j] <= life_) {
          state = {i, j};
          break;
        }
      }
    }

    // path 记录每一段空白是怎么过来的
    std::vector<int> path;
    while (state.first > 0) {
      auto ff = fr[state.first][state.second];
      path.push_back(ff.second);
      --state.first;
      state.second = ff.first;
    }

    // 一个也读不出来直接 jump
    if (path.empty()) {
      disk.Jump(time, target);               // 跳转到目标位置
      printer::ReadSetJump(disk_id, target); // 打印跳转信息
      return;
    }

    for (int i = 1; i <= task_cnt && !path.empty(); i++) {
      auto op = path.back();
      path.pop_back();
      int len = two_block_dist(task_k[i - 1], task_k[i]) - 1;
      if (op == 1) {
        printer::ReadAddRead(disk_id, len);
        while (len-- > 0) {
          auto [oid, y] = disk.GetStorageAt(disk.itr_);
          scheduler_->Update(oid, y); // 更新调度器信息
          disk.Read(time);
        }
      } else {
        disk.Pass(time, len);
        printer::ReadAddPass(disk_id, len);
      }

      auto [oid, y] = disk.GetStorageAt(disk.itr_);
      scheduler_->Update(oid, y); // 更新调度器信息
      disk.Read(time);
      printer::ReadAddRead(disk_id, 1);
    }

    // 就算走不到下一个目标，也可以选择继续移动「方案选单」
    // @ todo

    ReadSingle(disk_id, time);
#endif
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

  auto GetDisk(int disk_id) -> Disk & { return disks_[disk_id]; } // 获取磁盘
private:
  const int disk_cnt_;      // 磁盘数量
  const int life_;          // 磁盘生命周期
  ObjectPool *obj_pool_;    // 对象池
  Scheduler *scheduler_;    // 调度器
  SegmentManager *seg_mgr_; // 段管理器
  std::vector<Disk> disks_; // 磁盘列表
};