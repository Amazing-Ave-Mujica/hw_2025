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
extern int timeslice; // 全局变量，表示时间片
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
  // 将块 ID 添加到队列中
  void Push(int x) { st_.insert(x); }

  // 从队列中移除指定块 ID
  void Remove(int x) { st_.erase(x); }

  // 获取队列中最接近指定位置的块 ID
  // 参数：
  // - pos: 当前磁盘指针的位置
  // 返回值：最接近的块 ID，如果队列为空则返回 -1
  auto Front(int pos) -> int {
    if (st_.empty()) {
      return -1; // 队列为空
    }
    auto it = st_.lower_bound(pos); // 找到第一个大于等于 pos 的块
    if (it == st_.end()) {
      it = st_.begin(); // 如果没有找到，则从头开始
    }
    return *it;
  }

  // 获取队列的大小
  auto GetSize() -> int { return st_.size(); }

private:
  std::set<int> st_; // 使用有序集合维护块 ID，支持快速查找和删除
};

// 每个对象一个 TaskManager，用于管理对象的任务
class TaskManager {
public:
  // 构造函数
  // 参数：
  // - n: 对象的块数
  explicit TaskManager(int n) : mask_((1 << n) - 1) { l_.resize(mask_ + 1); }

  // 添加新任务
  // 参数：
  // - ptr: 指向任务的智能指针
  void NewTask(std::unique_ptr<Task> ptr) {
    l_[0].emplace_back(std::move(ptr)); // 将任务添加到初始状态
  }

  // 完成所有任务
  void Finish() {
    while (!l_[mask_].empty()) { // 遍历所有已完成的任务
      for (auto &p : l_[mask_]) {
        printer::AddReadRequest(p->tid_); // 将任务 ID 添加到读取请求
      }
      l_[mask_].clear(); // 清空已完成任务列表
    }
  }

  // 更新任务状态
  // 参数：
  // - x: 已完成的块编号
  void Update(int x) {
    assert((1 << x) <= mask_); // 确保块编号合法
    for (int i = 0; i <= mask_; i++) {
      if (((i >> x) & 1) == 0) { // 如果块 x 未完成
        assert((i | (1 << x)) != i);
        l_[i | (1 << x)].splice(l_[i | (1 << x)].end(), l_[i]); // 更新状态
        assert(l_[i].empty());
      }
    }
    Finish(); // 检查是否有任务完成
  }

  // 清空所有任务
  void Clear() {
    for (int i = 0; i <= mask_; i++) {
      l_[i].clear();
    }
  }

  // 友元函数，用于打印删除的对象
  friend void printer::AddDeleteObject(TaskManager &t);

private:
  int mask_; // 状态掩码，用于表示块的读取状态
  // 状态压缩，最多支持 32 种块的读/未读状态
  std::vector<std::list<std::unique_ptr<Task>>> l_;
};

namespace printer {
// 打印删除的对象
void AddDeleteObject(TaskManager &t) {
  for (const auto &l : t.l_) {
    for (const auto &p : l) {
      AddDeletedRequest(p->tid_); // 添加删除请求
    }
  }
}
} // namespace printer

// 调度器类，用于管理磁盘的读取任务
class Scheduler {
public:
  // 构造函数
  // 参数：
  // - obj_pool: 对象池，用于管理对象
  // - N: 磁盘数量
  // - T: 时间片数量
  explicit Scheduler(ObjectPool *obj_pool, int N, int T) : obj_pool_(obj_pool) {
    task_mgr_.reserve(T + 105); // 预留任务管理器的空间
    q_.resize(N + N);           // 初始化每个磁盘的读取队列
  }

  // 创建新的任务管理器
  // 参数：
  // - oid: 对象 ID
  // - size: 对象的块数
  void NewTaskMgr(int oid, int size) {
    assert(oid == task_mgr_.size()); // 确保对象 ID 与任务管理器的大小一致
    task_mgr_.emplace_back(size);    // 创建新的任务管理器
  }

  // 添加新任务
  // 参数：
  // - oid: 对象 ID
  // - ptr: 指向任务的智能指针
  void NewTask(int oid, std::unique_ptr<Task> ptr) {
    assert(oid < task_mgr_.size()); // 确保对象 ID 合法
    task_mgr_[oid].NewTask(std::move(ptr)); // 添加任务到对应的任务管理器
  }

  // 将块 ID 添加到指定磁盘的读取队列
  // 参数：
  // - disk_id: 磁盘 ID
  // - block_idck_id: 块 ID
  void PushRTQ(int disk_id, int block_idck_id) {
    q_[disk_id].Push(block_idck_id);
  }

  // 获取指定磁盘的下一个读取块
  // 参数：
  // - disk_id: 磁盘 ID
  // - pos: 当前磁盘指针的位置
  // 返回值：下一个读取块的 ID
  auto GetRT(int disk_id, int pos) -> int { return q_[disk_id].Front(pos); }

  // 获取指定磁盘的读取队列大小
  // 参数：
  // - disk_id: 磁盘 ID
  // 返回值：读取队列的大小
  auto GetRTQSize(int disk_id) -> int { return q_[disk_id].GetSize(); }

  // 删除对象的所有任务
  // 参数：
  // - oid: 对象 ID
  void Delete(int oid) {
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i];
      for (auto y : object->tdisk_[i]) {
        q_[disk_id].Remove(y); // 从读取队列中移除块
      }
    }
    printer::AddDeleteObject(task_mgr_[oid]); // 打印删除的对象
    task_mgr_[oid].Clear(); // 清空任务管理器
  }

  // 更新任务状态
  // 参数：
  // - oid: 对象 ID
  // - y: 已完成的块编号
  void Update(int oid, int y) {
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i];
      q_[disk_id].Remove(object->tdisk_[i][y]); // 从读取队列中移除块
    }
    task_mgr_[oid].Update(y); // 更新任务状态
  }

private:
  /*
    需要一个数据结构来给每个磁盘单独维护读队列
    支持：
      按照某种方法排序（如先进先出）
      删除其中某个属性 = xxx 的所有元素
    （用于其他磁盘读取了同样的块需要删，或者删除了某个 object）
  */
  ObjectPool *obj_pool_;              // 对象池，用于管理对象
  std::vector<RTQ> q_;                // 每个磁盘的读取队列
  std::vector<TaskManager> task_mgr_; // 每个对象的任务管理器
};