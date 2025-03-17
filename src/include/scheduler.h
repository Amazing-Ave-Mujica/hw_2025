#include <list>
#include <vector>
#include <memory>
#include "task.h"

class Scheduler {
public:
  explicit Scheduler(int N) {
    
  }

  void ReadRequest(int did, int oid, int blo) {
    
  }

  void DeleteRequest(int oid) {

  }

  // 给 disk_mgr 调用的接口，提示读取了某个块，需要更新读队列
  // 同时应该更新 task，看看是不是读完了，把 valid 设为 false
  void DiskReadBlock();

private:
  /*
    需要一个数据结构来给每个磁盘单独维护读队列
    支持：
      按照某种方法排序（如先进先出）
      删除其中某个属性 = xxx 的所有元素 （用于其他磁盘读取了同样的块需要删，或者删除了某个 objct）
  */
  // std::vector<std::list<int>> q_; 
  // std::vector<std::vector<std::list<int>::iterator>> vev_;
};