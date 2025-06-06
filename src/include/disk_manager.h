#pragma once

#include "config.h"
#include "disk.h"
#include "disk_manager.h"
#include "object.h"
#include "printer.h"
#include "scheduler.h"
#include "segment.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <numeric>
#include <queue>
#include <random>
#include <utility>
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
              SegmentManager *seg_mgr, std::vector<std::vector<db>> alpha,
              int N, int M, int V, int G, int K)
      : disk_cnt_(N), life_(G), obj_pool_(obj_pool), scheduler_(scheduler),
        alpha_(std::move(alpha)), seg_mgr_(seg_mgr), tag_sf_(M) {
    std::iota(tag_sf_.begin(), tag_sf_.end(), 0);
    disks_.reserve(disk_cnt_); // 预留磁盘数量的空间
    mirror_disks_.reserve(disk_cnt_ + disk_cnt_);
    for (int i = 0; i < disk_cnt_; i++) {
      disks_.emplace_back(i, V); // 初始化每个磁盘
      mirror_disks_.emplace_back(&disks_[i], V);
    }
    for (int i = 0; i < disk_cnt_; i++) {
      mirror_disks_.emplace_back(&disks_[i], V);
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
    std::vector<int> sf(config::REAL_DISK_CNT);
    std::iota(sf.begin(), sf.end(), 0);
    std::shuffle(sf.begin(), sf.end(), rng);
    // std::vector<int> tag_sf(disks_.size());
    // std::iota(tag_sf.begin(), tag_sf.end(), 0);
    sort(tag_sf_.begin(), tag_sf_.end(), [&](int a, int b) {
      return alpha_[object->tag_][a] > alpha_[object->tag_][b];
    });
    int tag;

    auto write_to_disk = [&](int od, const std::function<bool(int, int)> &check,
                             const std::function<bool(int, int)> &write) {
      if (!check(od, kth)) {
        return false; // 如果检查失败，返回 false
      }
      auto f1 = write(od, kth);
      assert(f1);
      return true;
    };

    // 按块写入数据

    auto check_by_block = [&](int od, int kth) {
      auto &disk = disks_[od];
      if constexpr (config::WritePolicy() == config::WRITEPOLICIES::compact) {
        if ((kth != 0 && (disk.GetFreeSize() - seg_mgr_->FreeBlockSize(od) <
                          object->size_)) ||
            (kth == 0 && disk.free_size_ < object->size_)) {
          return false; // 如果磁盘空间不足，返回 false
        }
      } else {
        if (disk.free_size_ < object->size_) {
          return false; // 如果磁盘空间不足，返回 false
        }
      }
      bool exist = false;
      for (int i = 0; i < kth; i++) {
        exist |= (object->idisk_[i] == disk.disk_id_); // 检查是否已存在副本
      }
      return !exist; // 如果不存在副本，返回 true
    };

    auto write_by_block = [&](int od, int kth) {
      auto &disk = disks_[od];
      object->idisk_[kth] = disk.disk_id_; // 设置副本所在磁盘
      for (int j = 0; j < object->size_; j++) {
        int block_id = -1;
        if constexpr (config::WritePolicy() == config::compact) {
          block_id =
              (kth > 0)
                  ? disk.WriteBlock(seg_mgr_->seg_disk_capacity_[od], oid, j)
                  : disk.WriteBlock(0, oid, j);
          object->tdisk_[kth][j] = block_id; // 记录块 ID

        } else {
          block_id = disk.WriteBlock(0, oid, j); // 写入数据到磁盘
          object->tdisk_[kth][j] = block_id;     // 记录块 ID
        }
        for (int i = 0, len = seg_mgr_->segs_.size(); i < len; i++) {
          auto ptr = seg_mgr_->FindBlock(i, od, block_id);
          if (ptr != nullptr) {
            seg_mgr_->Write(ptr, 1); // 更新段信息
          }
        }
      }
      return true;
    };

    auto check_by_block_forced = [&](int od, int kth) {
      auto &disk = disks_[od];
      if (disk.free_size_ < object->size_) {
        return false;
      }
      bool exist = false;
      for (int i = 0; i < kth; i++) {
        exist |= (object->idisk_[i] == disk.disk_id_); // 检查是否已存在副本
      }
      return !exist; // 如果不存在副本，返回 true
    };

    auto write_by_block_forced = [&](int od, int kth) {
      auto &disk = disks_[od];
      object->idisk_[kth] = disk.disk_id_; // 设置副本所在磁盘
      for (int j = 0; j < object->size_; j++) {
        {
          auto block_id = disk.WriteBlock(0, oid, j); // 写入数据到磁盘
          object->tdisk_[kth][j] = block_id;          // 记录块 ID
          for (int i = 0, len = seg_mgr_->segs_.size(); i < len; i++) {
            auto ptr = seg_mgr_->FindBlock(i, od, block_id);
            if (ptr != nullptr) {
              seg_mgr_->Write(ptr, 1); // 更新段信息
            }
          }
        }
      }
      return true; // 写入成功
    };

    auto check_by_segment = [&](int od, int kth) {
      auto &disk = disks_[od];
      auto ptr = seg_mgr_->Find(tag, od, object->size_); // 查找合适的段
      if (ptr == nullptr) {
        return false; // 如果没有找到合适的段，返回 false
      }
      bool exist = false;
      for (int i = 0; i < kth; i++) {
        exist |= (object->idisk_[i] == disk.disk_id_); // 检查是否已存在副本
      }
      return !exist; // 如果不存在副本，返回 true
    };

    auto write_by_segment = [&](int od, int kth) {
      auto &disk = disks_[od];
      auto ptr = seg_mgr_->Find(tag, od, object->size_); // 查找合适的段
      object->idisk_[kth] = od; // 设置副本所在磁盘
      for (int j = 0; j < object->size_; j++) {
        auto &block_id = object->tdisk_[kth][j];
        block_id = disk.WriteBlock(ptr->disk_addr_, oid, j); // 写入数据到段
      }
      seg_mgr_->Write(ptr, object->size_); // 更新段信息
      return true;                         // 写入成功
    };

    if (kth == 0) {
      for (int i : tag_sf_) {
        tag = i;
        for (auto od : sf) {
          if (kth == 0 &&
              write_to_disk(od, check_by_segment, write_by_segment)) {
            return true;
          }
        }
      }
    }
    for (auto od : sf) {
      if (write_to_disk(od, check_by_block, write_by_block)) {
        return true;
      }
    }
    for (auto od : sf) {
      if (write_to_disk(od, check_by_block_forced, write_by_block_forced)) {
        return true;
      }
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
    auto &disk = mirror_disks_[disk_id];
    while (time > 0) {
      const auto x =
          scheduler_->GetRT(disk_id, disk.GetItr()); // 获取调度器推荐的读取位置
      if (x == -1) {
        break; // 如果没有推荐位置，退出
      }
      if (disk.GetItr() == x) {
        break;
      }
      int rd_cost = disk.ReadCost();
      if (ReadDist(disk_id, x) <= 12) {
        if (time >= rd_cost) {
          auto [oid, y] = disk.GetStorageAt(disk.GetItr());
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

  void Read(int disk_id, int ext_g) {
    const int real_life = life_ + ext_g;
    int time = life_ + ext_g; // 初始化读取时间

#ifdef SINGLE_READ_MODE
    ReadSingle(disk_id, time);
#else
    auto &disk = mirror_disks_[disk_id];

    // 读取最近的 k 个任务，按距离升序存储
    auto task_k =
        scheduler_->GetRTK(disk_id, disk.GetItr(), config::DISK_READ_FETCH_LEN);

    if (task_k.empty()) {
      return;
    }
    // 如果最近的任务都太远，就直接 jump
    int target = scheduler_->GetRT(disk_id, disk.GetItr());
    if (ReadDist(disk_id, task_k[0]) >= real_life) {

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
                                         42, 52, 64, real_life + 1};
    // +1 是因为会有一个伪装任务
    std::vector<std::vector<int>> dp(task_cnt + 1,
                                     std::vector<int>(val.size(), real_life + 1));
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
      if (dp[i][val_id] > res && res <= real_life) {
        dp[i][val_id] = res;
        fr[i][val_id] = {pre_val_id, op};
      }
    };

    for (int i = 1; i <= task_cnt; i++) {
      for (int j = 0; j < val.size(); j++) {
        if (dp[i - 1][j] <= real_life) {
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
        if (dp[i][j] <= real_life) {
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
    auto &disk = mirror_disks_[disk_id];
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
  auto GetReadCount(int disk_id) -> int {
    return mirror_disks_[disk_id].read_count_; // 获取读取次数
  }

  // x -> y
  auto Trans(int disk_id, int x, int y) -> void {
    // return;
    // std::cerr<<"StartSwap\n";
    printer::GCAdd(disk_id, x, y);
    // std::cerr<<"GCAdd\n";
    auto &disk = disks_[disk_id];

    auto [oid, idx] = disk.GetStorageAt(x);

    scheduler_->Trans(disk_id, x, y, oid, idx);

    disk.Trans(x, y); // 磁盘交换数据
    // std::cerr<<"OK\n";
    seg_mgr_->Trans(disk_id, x, y); // 更新段信息
    // std::cerr<<"OK\n";
    // std::cerr<<"OK\n";
  }
  auto GarbageCollection(int k) -> void {

    // return;

    if constexpr (config::WritePolicy() == config::WRITEPOLICIES::compact) {
      // static bool f = false;
      // static std::vector<std::vector<int>> idx(disk_cnt_);
      // [&]() {
      //   if (f) {
      //     return;
      //   }
      //   for (int i = 0, len = tag_sf_.size(); i < len; i++) {
      //     for (auto &s : seg_mgr_->segs_[i]) {
      //       idx[s.disk_id_].push_back(s.disk_addr_);
      //     }
      //   }
      //   for (int i = 0; i < disk_cnt_; i++) {
      //     idx[i].emplace_back(seg_mgr_->seg_disk_capacity_[i]);
      //   }
      //   for (auto &v : idx) {
      //     std::sort(v.begin(), v.end());
      //   }
      //   f = true;
      // }();
      std::vector<int> lef(disk_cnt_, k);
      for (auto &seg_list : seg_mgr_->segs_) {
        for (auto &seg : seg_list) {
          auto &disk = disks_[seg.disk_id_];
          for (int i = seg.disk_addr_, j = seg.disk_addr_ + seg.size_ - 1;
               i < j; i++) {
            if (disk.storage_[i].first != -1) {
              continue;
            }
            while (i < j && disk.storage_[j].first == -1) {
              j--;
            }
            if (i >= j) {
              break;
            }
            if (lef[seg.disk_id_]-- > 0) {
              Trans(seg.disk_id_, j, i);
            }
          }
        }
      }
    }
  }

private:
  const int disk_cnt_;                   // 磁盘数量
  const int life_;                       // 磁盘生命周期
  ObjectPool *obj_pool_;                 // 对象池
  Scheduler *scheduler_;                 // 调度器
  SegmentManager *seg_mgr_;              // 段管理器
  std::vector<Disk> disks_;              // 磁盘列表
  std::vector<MirrorDisk> mirror_disks_; // 虚拟磁盘
  std::vector<std::vector<db>> alpha_;   // 相似矩阵
  std::vector<int> tag_sf_;              // 标签排序
};