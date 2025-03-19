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

  void Delete(int did, int blo) { disks_[did].Delete(blo); }

  void Read(int did) {
    int time = life_;
    auto &disk = disks_[did];
    while (time > 0) {
      const auto x = scheduler_->GetRT(did);
      if (x == -1) {
        break;
      }
      if (disk.itr_ == x) {
        if (disk.ReadCost() <= time) {
          disk.Read(time);
          printer::ReadAddRead(did, 1);
          assert(x >= 0 && x < disk.capacity_);
          auto [oid, y] = disk.GetStorageAt(x);
          scheduler_->Update(oid, y);
        }
        break;
      }
      // disk.Pass(time);
      // printer::ReadAddPass(did, 1);
      assert(time == life_);
      disk.Jump(time, x);
      printer::ReadSetJump(did, x + 1);
    }
  }

private:
  const int disk_cnt_;
  const int life_;
  ObjectPool *obj_pool_;
  Scheduler *scheduler_;
  std::vector<Disk> disks_;
};