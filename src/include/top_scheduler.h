#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <memory>
#include "scheduler.h"
#include "task.h"
#include "object.h"
#include "disk_manager.h"

class TopScheduler {
public:

  TopScheduler(int *time, Scheduler *scheduler, ObjectPool* obj_pool, DiskManager *disk_mgr) : time_(time), scheduler_(scheduler), obj_pool_(obj_pool), disk_mgr_(disk_mgr) {};

  // send to scheduler
  void ReadRequest(int tid, int oid)  {
    auto object = obj_pool_->GetObjAt(oid);
    scheduler_->NewTask(oid, std::make_unique<Task>(tid, oid, object->size_, *time_));
    // 简单挑选其中两个磁盘读
    int x = rand() % 3;
    int y = rand() % 3;
    for (int i = 0; i < (object->size_ >> 1); i++) {
      scheduler_->PushRTQ(x, object->tdisk_[x][i]);
    }
    for (int i = (object->size_ >> 1); i < object->size_; i++) {
      scheduler_->PushRTQ(y, object->tdisk_[y][i]);
    }
  }

  // send to disk_mgr directly
  void InsertRequest(int id, int size, int tag) {
    int oid = obj_pool_->NewObject(id, tag, size);
    assert(id == oid);
    scheduler_->NewTaskMgr(oid, size);
    for (int i = 0; i < 3; i++) {
      disk_mgr_->Insert(oid, i);
    }
  }
  
  /*
  delete some read_request 
  send to scheduler
  */
  void DeleteRequest(int oid) {
    scheduler_->Delete(oid);
    auto object = obj_pool_->GetObjAt(oid);
    for (int i = 0; i < 3; i++) {
      int did = object->idisk_[i];
      for (auto y : object->tdisk_[i]) {
       disk_mgr_->Delete(did, y);
      }
    }
  }

  void Read() {
    for (int i = 0; i < disk_mgr_->GetDiskCnt(); i++) {
      disk_mgr_->Read(i);
    }
  }


private:
  int *time_;
  Scheduler *scheduler_;
  ObjectPool *obj_pool_;
  DiskManager *disk_mgr_;
};