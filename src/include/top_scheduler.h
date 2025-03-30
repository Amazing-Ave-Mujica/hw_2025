#include "config.h"
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
  TopScheduler(int *time, Scheduler *scheduler, ObjectPool *obj_pool,
               DiskManager *disk_mgr)
      : time_(time), scheduler_(scheduler), obj_pool_(obj_pool),
        disk_mgr_(disk_mgr){};

  // 处理读取请求
  // 参数：
  // - tid: 读取任务的唯一标识符
  // - oid: 读取任务关联的对象 ID
  void ReadRequest(int tid, int oid) {
    assert(oid >= 0);                       // 确保对象 ID 合法
    assert(obj_pool_->IsValid(oid));        // 确保对象有效
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    scheduler_->NewTask(oid,
                        std::make_unique<Task>(tid, oid, *time_)); // 创建新任务

    int x; // 选择的副本索引
    if constexpr (config::WritePolicy() == config::none) {
      std::vector<int> v{0, 1, 2}; // 副本索引列表
      // 根据磁盘压力排序，选择压力最小的磁盘
      std::sort(v.begin(), v.end(), [&](int x, int y) {
        return disk_mgr_->GetStress(object->idisk_[x], object->tdisk_[x][0]) <
               disk_mgr_->GetStress(object->idisk_[y], object->tdisk_[y][0]);
      });
      x = v[0]; // 选择压力最小的副本
    } else if constexpr (config::WritePolicy() == config::compact) {
      x = 0;
    }
    int disk = object->idisk_[x]; // 获取选择的磁盘 ID
    for (int i = 0; i < object->size_; i++) {
      scheduler_->PushRTQ(disk, object->tdisk_[x][i]); // 将块 ID 添加到读取队列
    }
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
      assert(disk_mgr_->Insert(oid, i)); // 将对象的副本插入磁盘
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
  void Read() {
    for (int i = 0; i < disk_mgr_->GetDiskCnt(); i++) {
      disk_mgr_->Read(i); // 调用磁盘管理器的读取操作
    }
  }

private:
  int *time_;             // 指向全局时间片的指针
  Scheduler *scheduler_;  // 调度器，用于管理任务
  ObjectPool *obj_pool_;  // 对象池，用于管理对象
  DiskManager *disk_mgr_; // 磁盘管理器，用于管理磁盘操作
};