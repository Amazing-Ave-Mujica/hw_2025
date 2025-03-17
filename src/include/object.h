#include <cassert>
#include <vector>
#include <memory>
#include <array>

struct Object {
  Object(int id, int tag, int size) : id_(id), tag_(tag), size_(size) {}
  
  std::array<int, 3> idisk_;
  int id_;
  int tag_;
  int size_;
};

class ObjectPool {
public:
  explicit ObjectPool(int T) {
    objs_.reserve(T + 105);
  }

  auto NewObject(int id, int tag, int size) -> int {
    objs_.emplace_back(std::make_shared<Object>(id, tag, size));
    return size_++;
  }

  auto GetObjAt(int oid) -> std::shared_ptr<Object> {
    assert(oid >= 0 && oid < size_);
    return objs_[oid];
  }

  void SetIdisk(int oid, int idx, int disk_idx) {
    objs_[oid]->idisk_[idx] = disk_idx;
  }

private:
  int size_;
  std::vector<std::shared_ptr<Object>> objs_;
};