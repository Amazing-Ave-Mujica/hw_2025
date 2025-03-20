#pragma once

#include "disk.h"
#include "object.h"
#include "printer.h"
#include "scheduler.h"
#include <algorithm>
#include <random>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice;
#endif

class DiskManager {
public:
  DiskManager(ObjectPool *obj_pool, Scheduler *scheduler, int N, int V, int G)
      : disk_cnt_(N), life_(G), obj_pool_(obj_pool), scheduler_(scheduler) {
    disks_.reserve(disk_cnt_);
    for (int i = 0; i < disk_cnt_; i++) {
      disks_.emplace_back(i, V);
    }
  }

  auto GetDiskCnt() const -> int { return disk_cnt_; }

  // 插入 oid 的 kth 个副本
  auto Insert(int oid, int kth) -> bool {
    auto object = obj_pool_->GetObjAt(oid);
    std::vector<int> sf(disks_.size());
    std::iota(sf.begin(), sf.end(), 0);
    std::mt19937 rng(time(nullptr));
    std::shuffle(sf.begin(), sf.end(), rng);
    for (auto od : sf) {
      auto &disk = disks_[od];
      if (disk.free_size_ >= object->size_) {
        bool exist = false;
        for (int i = 0; i < kth; i++) {
          exist |= (object->idisk_[i] == disk.disk_id_);
        }
        if (!exist) {
          object->idisk_[kth] = disk.disk_id_;
          for (int j = 0; j < object->size_; j++) {
            object->tdisk_[kth][j] = disk.Write(oid, j);
          }
          return true;
        }
      }
    }
    return false;
  }

  void Delete(int disk_id, int block_id) { disks_[disk_id].Delete(block_id); }

  void Read(int disk_id) {
    int time = life_;
    auto &disk = disks_[disk_id];
    while (time > 0) {
      const auto x = scheduler_->GetRT(disk_id, disk.itr_);
      if (x == -1) {
        break;
      }
      if (disk.itr_ == x) {
        if (disk.ReadCost() <= time) {
          disk.Read(time);
          printer::ReadAddRead(disk_id, 1);
          assert(x >= 0 && x < disk.capacity_);
          auto [oid, y] = disk.GetStorageAt(x);
          scheduler_->Update(oid, y);
          continue;
        }
        break;
      }
      if (ReadDist(disk_id, x) >= life_ && time == life_) {
        disk.Jump(time, x);
        printer::ReadSetJump(disk_id, x + 1);
      } else {
        int rd_cost = disk.ReadCost();
        if (time >= rd_cost && ReadDist(disk_id, x) <= 20 && rd_cost <= 25) {
          auto [oid, y] = disk.GetStorageAt(disk.itr_);
          disk.Read(time);
          printer::ReadAddRead(disk_id, 1);
          if (oid >= 0) {
            scheduler_->Update(oid, y);
          }
        } else {
          disk.Pass(time);
          printer::ReadAddPass(disk_id, 1);
        }
      }
    }
  }

  auto ReadDist(int disk_id, int dest) -> int {
    auto &disk = disks_[disk_id];
    int siz = disk.capacity_;
    int pos = disk.itr_;
    return (dest - pos + siz) % siz; 
  }

  auto GetStress(int disk_id, int dest) -> int {
    return (ReadDist(disk_id, dest) * 3) + scheduler_->GetRTQSize(disk_id);
  }

private:
  const int disk_cnt_;
  const int life_;
  ObjectPool *obj_pool_;
  Scheduler *scheduler_;
  std::vector<Disk> disks_;
};