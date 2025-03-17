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

  TopScheduler(int *time, Scheduler *scheduler, ObjectPool* obj_pool, DiskManager *disk_mgr, int T) : time_(time), scheduler_(scheduler), obj_pool_(obj_pool), disk_mgr_(disk_mgr) {
    tasks_.resize(T + 105);
  }

  // send to scheduler
  void ReadRequest(int tid, int oid)  {
    auto object = obj_pool_->GetObjAt(oid);
    tasks_[oid].emplace_back(std::make_shared<Task>(tid, oid, object->size_, *time_));
    // 简单挑选其中两个磁盘读
    int x = rand() % 3;
    int y = rand() % 3;
    for (int i = 0; i < (object->size_ >> 1); i++) {
      scheduler_->ReadRequest(x, oid, i);
    }
    for (int i = (object->size_ >> 1); i < object->size_; i++) {
      scheduler_->ReadRequest(y, oid, i);
    }
  }

  // send to disk_mgr directly
  void InsertRequest(int id, int size, int tag) {
    int oid = obj_pool_->NewObject(id, tag, size);
    for (int i = 0; i < 3; i++) {
      obj_pool_->SetIdisk(oid, i, disk_mgr_->Insert(oid));
    }
  }
  
  /*
  delete some read_request 
  send to scheduler
  */
  void DeleteRequest(int oid) {
    tasks_[oid].clear();
    scheduler_->DeleteRequest(oid);
  }


private:
  int *time_;
  Scheduler *scheduler_;
  ObjectPool *obj_pool_;
  DiskManager *disk_mgr_;
  std::vector<std::vector<std::unique_ptr<Task>>> tasks_;  
};