#pragma once

#include "config.h"
#include "disk.h"
#include "object.h"
#include "printer.h"
#include "task.h"
#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
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
class RTQ {
private:
  void UpdateSum(int x, int v = 1) {
    auto it = std::upper_bound(sum_.begin(), sum_.end(),
                               std::make_pair(x, INT32_MAX));
    assert(it != sum_.begin());
    it = prev(it);
    it->second += v;
    read_stress_ += v;
  }

public:
  // 构造函数
  explicit RTQ(int V) {
    for (int i = 0; i < V; i += V) {
      sum_.emplace_back(i, 0);
    }
    sum_.emplace_back(V, 0);
  }

  // 将块 ID 添加到队列中
  void Push(int x) {
    st_.insert(x);
    ++cnt_[x];
    UpdateSum(x);
  }

  // 从队列中移除指定块 ID
  void Remove(int x) {
    if (cnt_.count(x) == 0) {
      return;
    }
    UpdateSum(x, -cnt_[x]);
    st_.erase(x);
    cnt_.erase(x);
  }

  // 删除一次请求
  void RemoveOnce(int x) {
    if (cnt_.count(x) == 0) {
      return;
    }
    UpdateSum(x, -1);
    if (--cnt_[x] == 0) {
      st_.erase(x);
      cnt_.erase(x);
    }
  }

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

  auto FrontK(int pos, int k) -> std::vector<int> {
    if (st_.empty()) {
      return {};
    }
    auto it = st_.lower_bound(pos); // 找到第一个大于等于 pos 的块
    std::vector<int> res;
    res.reserve(k);
    for (; it != st_.end() && k > 0; ++it) {
      res.push_back(*it);
      --k;
    }
    // 再从头开始找，因为磁盘是布局一个环
    for (it = st_.begin(); it != st_.end() && *it < pos && k > 0; ++it) {
      res.push_back(*it);
      --k;
    }
    return res;
  }

  // 获取队列的大小
  auto GetSize() -> int { return st_.size(); }

  // 得到最热门的块
  auto GetHotBlock() -> std::pair<int, int> {
    int mx = 0;
    int pos = 0;
    for (const auto &[x, y] : sum_) {
      if (mx < y) {
        pos = x;
        mx = y;
      }
    }
    return {Front(pos), mx};
  }

  // 查询块查询次数
  auto QueryBlockCnt(int pos) -> int {
    auto it = std::upper_bound(sum_.begin(), sum_.end(),
                               std::make_pair(pos, INT32_MAX));
    assert(it != sum_.begin());
    it = prev(it);
    return it->second;
  }

  auto QueryReadStress() -> int {
    return read_stress_; // 返回读取压力
  }

  auto Trans(int x, int y) -> void {
    if (st_.find(x) == st_.end()) {
      return;
    }
    assert(st_.find(y) == st_.end());
    int tmp = cnt_[x];
    Remove(x);
    st_.insert(y);
    cnt_[y] = tmp;
    UpdateSum(y, tmp);
  }

private:
  std::set<int> st_; // 使用有序集合维护块 ID，支持快速查找和删除
  std::vector<std::pair<int, int>> sum_; // 前缀和
  std::unordered_map<int, int> cnt_;     // 出现次数
  int read_stress_{0};                   // 读取压力
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
  void NewTask(std::shared_ptr<Task> ptr) {
    l_[0].emplace_back(std::move(ptr)); // 将任务添加到初始状态
  }

  // 完成所有任务
  void Finish() {
    if (!l_[mask_].empty()) { // 遍历所有已完成的任务
      for (const auto &p : l_[mask_]) {
        // 引用计数大于 1 说明任务没有先繁忙
        if (p.use_count() > 1) {
          assert(p->timestamp_ > timeslice - config::REQ_BUSY_TIME);
          printer::AddReadRequest(p->tid_); // 将任务 ID 添加到读取请求
        }
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
    valid_ = false;
    for (int i = 0; i <= mask_; i++) {
      l_[i].clear();
    }
  }

  void Trans(int x, int y) {
    for (int i = 0; i <= mask_; i++) {
      for (auto &task_list : l_) {
        for (auto &task : task_list) {
          task->TransWork(x, y);
        }
      }
    }
  }

  // 友元函数，用于打印删除的对象
  friend void printer::AddDeleteObject(TaskManager &t);

private:
  bool valid_{true}; // 表示对象是否已经被删除
  int mask_;         // 状态掩码，用于表示块的读取状态
  // 状态压缩，最多支持 32 种块的读/未读状态
  std::vector<std::list<std::shared_ptr<Task>>> l_;
};

namespace printer {
// 打印删除的对象
void AddDeleteObject(TaskManager &t) {
  for (const auto &l : t.l_) {
    for (const auto &p : l) {
      if (p.use_count() > 1) {
        AddDeletedRequest(p->tid_); // 添加删除请求
      }
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
  explicit Scheduler(ObjectPool *obj_pool, int N, int T, int V)
      : obj_pool_(obj_pool) {
    task_mgr_.reserve(T + 105); // 预留任务管理器的空间
    q_.resize(N + N, (RTQ){V}); // 初始化每个磁盘的读取队列
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
  void NewTask(int oid, const std::shared_ptr<Task> &ptr) {
    assert(oid < task_mgr_.size()); // 确保对象 ID 合法
    task_mgr_[oid].NewTask(ptr);    // 添加任务到对应的任务管理器
    req_list_.push_back(ptr);
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

  // 一次获取 k 个读任务
  auto GetRTK(int disk_id, int pos, int k) -> std::vector<int> {
    return q_[disk_id].FrontK(pos, k);
  }

  // 跳的时候获取最热门的块
  auto GetHotRT(int disk_id) -> std::pair<int, int> {
    return q_[disk_id].GetHotBlock();
  }

  auto GetCntRT(int disk_id, int pos) -> int {
    return q_[disk_id].QueryBlockCnt(pos);
  }

  // 获取指定磁盘的读取队列大小
  // 参数：
  // - disk_id: 磁盘 ID
  // 返回值：读取队列的大小
  auto GetRTQSize(int disk_id) -> int { return q_[disk_id].GetSize(); }
  auto GetReadStress(int disk_id) -> int {
    return q_[disk_id].QueryReadStress(); // 获取读取压力
  }
  // 删除对象的所有任务
  // 参数：
  // - oid: 对象 ID
  void Delete(int oid) {
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i];
      for (auto y : object->tdisk_[i]) {
        q_[disk_id].Remove(y); // 从读取队列中移除块
        q_[disk_id + config::REAL_DISK_CNT].Remove(
            y); // 从镜像磁盘的读取队列中移除块
      }
    }
    printer::AddDeleteObject(task_mgr_[oid]); // 打印删除的任务
    task_mgr_[oid].Clear();                   // 清空任务管理器
  }

  // 更新任务状态
  // 参数：
  // - oid: 对象 ID
  // - y: 已完成的块编号
  void Update(int oid, int y) {
    if (oid < 0) {
      return;
    }
    auto object = obj_pool_->GetObjAt(oid); // 获取对象
    for (int i = 0; i < 3; i++) {
      int disk_id = object->idisk_[i];
      q_[disk_id].Remove(object->tdisk_[i][y]); // 从读取队列中移除块
      q_[disk_id + config::REAL_DISK_CNT].Remove(
          object->tdisk_[i][y]); // 镜像磁盘也删了
    }
    task_mgr_[oid].Update(y); // 更新任务状态
  }

  // [?] 获取磁盘读任务的分布，用于读调度器使用
  void Trans(int disk_id, int x, int y, int oid, int idx) {
    // 修改 object
    auto obj = obj_pool_->GetObjAt(oid);
    for (int i = 0; i < 3; i++) {
      if (obj->idisk_[i] == disk_id) {
        obj->tdisk_[i][idx] = y;
        break;
      }
    }
    // 更新超时删除队列
    task_mgr_[oid].Trans(x, y);

    q_[disk_id].Trans(x, y);                         // 交换读取队列中的块
    q_[disk_id + config::REAL_DISK_CNT].Trans(x, y); // 镜像磁盘也交换
  }

  void PopOldReqs() {
    int lim = timeslice - config::REQ_BUSY_TIME;
    while (!req_list_.empty() && req_list_.front()->timestamp_ <= lim) {
      const auto &req = req_list_.front();
      // 如果对象之前没有被删除，到对应磁盘删除读请求
      if (obj_pool_->IsValid(req->oid_) && req.use_count() > 1) {
        for (const auto &[disk_id, block_id] : req->work_) {
          q_[disk_id].RemoveOnce(block_id);
        }
        printer::ReadAddBusy(req->tid_);
      }
      req_list_.pop_front();
    }
  }

private:
  /*
    需要一个数据结构来给每个磁盘单独维护读队列
    支持：
      按照某种方法排序（如先进先出）
      删除其中某个属性 = xxx 的所有元素
    （用于其他磁盘读取了同样的块需要删，或者删除了某个 object）
  */
  ObjectPool *obj_pool_;                      // 对象池，用于管理对象
  std::vector<RTQ> q_;                        // 每个磁盘的读取队列
  std::vector<TaskManager> task_mgr_;         // 每个对象的任务管理器
  std::list<std::shared_ptr<Task>> req_list_; // 支持删除 105 个时间片前的任务
};