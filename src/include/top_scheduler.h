#include "config.h"
#include "disk.h"
#include "disk_manager.h"
#include "object.h"
#include "scheduler.h"
#include "task.h"
#include <cstdlib>
#include <memory>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice; // 全局变量，表示时间片
#endif

// 顶层调度器类，用于管理对象的读写请求和磁盘操作
class TopScheduler {
public:
  // 构造函数
  // 参数：
  // - time: 指向全局时间片的指针
  // - scheduler: 调度器，用于管理任务
  // - obj_pool: 对象池，用于管理对象
  // - disk_mgr: 磁盘管理器，用于管理磁盘操作
  TopScheduler(Scheduler *scheduler, ObjectPool *obj_pool,
               DiskManager *disk_mgr, int V)
      : scheduler_(scheduler), obj_pool_(obj_pool), disk_mgr_(disk_mgr),
        v_(V){};

  // 处理读取请求
  // 参数：
  // - tid: 读取任务的唯一标识符
  // - oid: 读取任务关联的对象 ID
  void ReadRequest(int tid, int oid) {
    assert(oid >= 0);                       // 确保对象 ID 合法
    assert(obj_pool_->IsValid(oid));        // 确ee保对象有效
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    int disk;                               // 选择读的磁盘
    std::vector<std::pair<int, int>> v;     // 候选磁盘列表
    if constexpr (config::WritePolicy() == config::none) {
      v.reserve(6);
      for (int i = 0; i < 3; i++) {
        v.emplace_back(object->idisk_[i], object->tdisk_[i][0]);
        v.emplace_back(object->idisk_[i] + config::REAL_DISK_CNT,
                       object->tdisk_[i][0]);
      }
    } else if constexpr (config::WritePolicy() == config::compact) {
      v.reserve(1);
      int idx1 = object->idisk_[0];
      int idx2 = idx1 + config::REAL_DISK_CNT; // NOLINT
      if (object->tdisk_[0][0] < v_ / 6) {
        disk = idx1;
      } else {
        disk = idx2;
      }
    }
    // 根据磁盘压力排序，选择压力最小的磁盘
    // std::sort(v.begin(), v.end(), [&](auto x, auto y) {
    //   return disk_mgr_->GetStress(x.first, x.second) <
    //          disk_mgr_->GetStress(y.first, y.second);
    // });
    // disk = v[0].first; // 选择压力最小的副本

    int x;
    for (int i = 0; i < 3; i++) {
      if (object->idisk_[i] == disk ||
          object->idisk_[i] + config::REAL_DISK_CNT == disk) {
        x = i;
        break;
      }
    }

    std::vector<std::pair<int, int>> work;
    work.reserve(object->size_);
    // 怎么分给两个镜像呢
    for (int i = 0; i < object->size_; i++) {
      scheduler_->PushRTQ(disk, object->tdisk_[x][i]); // 将块 ID 添加到读取队列
      work.emplace_back(disk, object->tdisk_[x][i]);
    }
    scheduler_->NewTask(oid,
                        std::make_shared<Task>(tid, oid, timeslice,
                                               std::move(work))); // 创建新任务
  }

  // 处理插入请求
  // 参数：
  // - id: 对象的唯一标识符
  // - size: 对象的大小（块数）
  // - tag: 对象的标签
  // 返回值：新对象的 ID
  auto InsertRequest(int id, int size, int tag) -> int {
    int oid = obj_pool_->NewObject(id, tag, size); // 创建新对象
    assert(id == oid);                             // 确保对象 ID 一致
    scheduler_->NewTaskMgr(oid, size); // 创建新的任务管理器
    for (int i = 0; i < 3; i++) {
      auto scc = disk_mgr_->Insert(oid, i);
      assert(scc); // 将对象的副本插入磁盘
    }
    return oid; // 返回新对象的 ID
  }

  /*
  delete some read_request
  send to scheduler
  */
  // 处理删除请求
  // 参数：
  // - oid: 要删除的对象 ID
  void DeleteRequest(int oid) {
    scheduler_->Delete(oid); // 从调度器中删除任务

    // 磁盘删除
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i]; // 获取副本所在的磁盘 ID
      for (auto y : object->tdisk_[i]) {
        disk_mgr_->Delete(object->tag_, disk_id, y); // 从磁盘中删除块
      }
    }

    obj_pool_->Drop(oid); // 将对象标记为无效
  }

  // 执行读取操作
  void Read(int ext_g) {
    scheduler_->PopOldReqs();
    for (int i = 0; i < (disk_mgr_->GetDiskCnt() << 1); i++) {
      disk_mgr_->Read(i, ext_g); // 调用磁盘管理器的读取操作
    }
  }

private:
  Scheduler *scheduler_;  // 调度器，用于管理任务
  ObjectPool *obj_pool_;  // 对象池，用于管理对象
  DiskManager *disk_mgr_; // 磁盘管理器，用于管理磁盘操作
  int v_;
};