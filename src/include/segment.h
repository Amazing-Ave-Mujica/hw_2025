#include <algorithm>
#include <list>
#include <vector>
#include "data.h"
class Segment {
public:
  static constexpr int DEFAULT_CAPACITY = 10;
  int disk_id_, disk_addr_, tag_, capacity_, size_{0};
  Segment(int disk_id, int disk_addr, int tag, int capacity = DEFAULT_CAPACITY)
      : disk_id_(disk_id), disk_addr_(disk_addr), tag_(tag),
        capacity_(capacity) {}
  void Expand(int len){
    capacity_ += std::min(len,capacity_);
  }
  void Write(int size) {
    size_ += size;
  }
  void Delete(int size ) {
    size_ -= size;
  }
};

class SegmentManager{
public:
  std::vector<std::list<Segment>> segs_;
  SegmentManager(int M,int N,int V,const Data& t) : segs_(M) {
    int sum = 0;
    std::vector<int> weights(M);
    for(int i = 0;i < M;i++){
      sum += t[i][0];
      weights[i] = t[i][0];
    }
   
    // 平均每一个 disk 按照第一个时间片的写入量 按权重分配磁盘空间
    for(int i = 0;i < N;i++){
      int addr = 0;
      int rem = V;
      for(int j = 0;j < M;j++){
        int64_t capacity = std::min (static_cast<int>((1LL * V  * weights[i] + sum - 1) / sum),rem); 
        segs_[j].emplace_back(i,addr,j,capacity);
        addr += capacity;
        rem -= capacity;
      }
    }
  }
  auto Find(int tag,int disk_id,int size) -> Segment* {
    for(auto& s : segs_[tag]){
      if (s.disk_id_ != disk_id){
        continue;
      }
      if (s.size_ + size <= s.capacity_){
        return &s;
      }
    }
    return nullptr;
  }
  auto Delete(int tag,int disk_id,int block_id){
    
  }
};