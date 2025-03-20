#pragma once

#include "object.h"
#include "printer.h"
#include "task.h"
#include <list>
#include <memory>
#include <set>
#include <vector>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice;
#endif

class TaskManager;

namespace printer {
void AddDeleteObject(TaskManager &t);
};

/**
1. 尽量一次读完某个 TAG 
2. 读某个 TAG 尽量让指针同方向移动
3. 设置时间片轮转机制防止饥饿
*/
struct RTQ {
public:
  void Push(int x) {
    st_.insert(x);
  }

  void Remove(int x) {
    st_.erase(x);
  }

  auto Front(int pos) -> int {
    if (st_.empty()) {
      return -1;
    }
    auto it = st_.lower_bound(pos);
    if (it == st_.end()) {
      it = st_.begin();
    }
    return *it;
  }

  auto GetSize() -> int {
    return st_.size();
  }

private:
  std::set<int> st_;
};

// 每个 object 一个 TaskManager
class TaskManager {
public:
  explicit TaskManager(int n) : mask_((1 << n) - 1) { l_.resize(mask_ + 1); }

  void NewTask(std::unique_ptr<Task> ptr) {
    l_[0].emplace_back(std::move(ptr));
  }
  // obj disk (x)
  // RTQ remove(obj)
  //
  void Finish() {
    while (!l_[mask_].empty()) {
      for (auto &p : l_[mask_]) {
        printer::AddReadRequest(p->tid_);
      }
      l_[mask_].clear();
    }
  }

  void Update(int x) {
    assert((1 << x) <= mask_);
    for (int i = 0; i <= mask_; i++) {
      if (((i >> x) & 1) == 0) {
        assert((i | (1 << x)) != i);
        l_[i | (1 << x)].splice(l_[i | (1 << x)].end(), l_[i]);
        assert(l_[i].empty());
      }
    }
    Finish();
  }

  void Clear() {
    for (int i = 0; i <= mask_; i++) {
      l_[i].clear();
    }
  }

  friend void printer::AddDeleteObject(TaskManager &t);

private:
  int mask_;
  // 状压 32 种块 读/未读 的状态
  std::vector<std::list<std::unique_ptr<Task>>> l_;
};

namespace printer {
void AddDeleteObject(TaskManager &t) {
  for (const auto &l : t.l_) {
    for (const auto &p : l) {
      AddDeletedRequest(p->tid_);
    }
  }
}
} // namespace printer

class Scheduler {
public:
  explicit Scheduler(ObjectPool *obj_pool, int N, int T) : obj_pool_(obj_pool) {
    task_mgr_.reserve(T + 105);
    q_.resize(N + N);
  }

  void NewTaskMgr(int oid, int size) {
    assert(oid == task_mgr_.size());
    task_mgr_.emplace_back(size);
  }

  void NewTask(int oid, std::unique_ptr<Task> ptr) {
    assert(oid < task_mgr_.size());
    task_mgr_[oid].NewTask(std::move(ptr));
  }

  void PushRTQ(int disk_id, int block_idck_id) { q_[disk_id].Push(block_idck_id); }

  auto GetRT(int disk_id, int pos) -> int { return q_[disk_id].Front(pos); }

  auto GetRTQSize(int disk_id) -> int {
    return q_[disk_id].GetSize();
  }

  void Delete(int oid) {
    auto object = obj_pool_->GetObjAt(oid);
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i];
      for (auto y : object->tdisk_[i]) {
        q_[disk_id].Remove(y);
      }
    }
    printer::AddDeleteObject(task_mgr_[oid]);
    task_mgr_[oid].Clear();
  }

  // 给 disk_mgr 调用的接口，当读了新的块的时候调用
  void Update(int oid, int y) {
    auto object = obj_pool_->GetObjAt(oid);
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i];
      q_[disk_id].Remove(object->tdisk_[i][y]);
    }
    task_mgr_[oid].Update(y);
  }

private:
  /*
    需要一个数据结构来给每个磁盘单独维护读队列
    支持：
      按照某种方法排序（如先进先出）
      删除其中某个属性 = xxx 的所有元素
    （用于其他磁盘读取了同样的块需要删，或者删除了某个 objct）
  */
  ObjectPool *obj_pool_;
  std::vector<RTQ> q_;
  std::vector<TaskManager> task_mgr_;
};