#pragma once

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

struct Object {
  Object(int id, int tag, int size) : id_(id), tag_(tag), size_(size) {
    for (auto &i : tdisk_) {
      i.resize(size_);
    }
  }

  int id_;
  int tag_;
  int size_;
  std::array<int, 3> idisk_;
  std::vector<int> tdisk_[3];
};

class ObjectPool {
public:
  explicit ObjectPool(int T) { objs_.reserve(T + 105); }

  auto NewObject(int id, int tag, int size) -> int {
    objs_.emplace_back(std::make_shared<Object>(id, tag, size));
    return size_++;
  }

  auto GetObjAt(int oid) -> std::shared_ptr<Object> {
   // std::cerr << oid << ' ' << size_ << '\n';
    assert(oid >= 0 && oid < size_);
    return objs_[oid];
  }

private:
  int size_{};
  std::vector<std::shared_ptr<Object>> objs_;
};