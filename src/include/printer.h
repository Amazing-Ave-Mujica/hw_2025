#include "config.h"
#include <cstdio>
#ifndef _PRINTER_H
#define _PRINTER_H
#include "object.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>

#ifndef _TIMESLICE
#define _TIMESLICE
extern int timeslice; // 全局变量，表示时间片
#endif

namespace printer {

// 缓冲区，用于存储不同类型的请求
int buf[3][config::PRINTER_BUF_CAPACITY]; // 三种请求类型的缓冲区，每种类型最多存储 2^20 个请求
int top[3] = {0};    // 每种请求缓冲区的当前大小
std::string ops[config::MAX_N]; // 每个磁盘的操作记录（最多支持 10 个磁盘）

// 请求类型的枚举
// DELETE: 删除请求
// WRITE: 写入请求
// READ: 读取请求
//NOLINTNEXTLINE
enum { DELETE = 0, WRITE, READ };

// 清空指定类型的缓冲区和操作记录
// 参数：
// - idx: 要清空的缓冲区索引
void Clean(int idx) {
  std::cout.flush(); // 刷新输出流
  top[idx] = 0;      // 重置缓冲区大小
  for (auto &op : ops) {
    op.clear(); // 清空操作记录
  }
}

// 添加读取请求到缓冲区
// 参数：
// - RequestID: 读取请求的 ID
void AddReadRequest(int RequestID) { buf[READ][top[READ]++] = RequestID; }

// 添加跳过操作到指定磁盘的操作记录
// 参数：
// - DiskNum: 磁盘编号
// - cnt: 跳过的次数
void ReadAddPass(size_t DiskNum, int cnt) {
  for (int i = 0; i < cnt; i++) {
    ops[DiskNum].push_back('p'); // 添加 'p' 表示跳过操作
  }
}

// 添加读取操作到指定磁盘的操作记录
// 参数：
// - DiskNum: 磁盘编号
// - cnt: 读取的次数
void ReadAddRead(size_t DiskNum, int cnt) {
  for (int i = 0; i < cnt; i++) {
    ops[DiskNum].push_back('r'); // 添加 'r' 表示读取操作
  }
}

// 设置跳转操作到指定磁盘的操作记录
// 参数：
// - DiskNum: 磁盘编号
// - DiskblockID: 跳转的块编号
void ReadSetJump(size_t DiskNum, size_t DiskblockID) {
  ops[DiskNum] = "j " + std::to_string(DiskblockID + 1); // 添加跳转操作
}

// 添加写入对象到缓冲区
// 参数：
// - ObjectID: 写入对象的 ID
void AddInsertedObject(int ObjectID) { buf[WRITE][top[WRITE]++] = ObjectID; }

// 添加删除请求到缓冲区
// 参数：
// - RequestID: 删除请求的 ID
void AddDeletedRequest(int RequestID) {
  buf[DELETE][top[DELETE]++] = RequestID;
}

// 打印删除请求
auto PrintDelete() -> void {
  std::cout << top[DELETE] << '\n'; // 打印删除请求的数量
  for (int i = 0; i < top[DELETE]; i++) {
    std::cout << buf[DELETE][i] << '\n'; // 打印每个删除请求的 ID
  }
  Clean(DELETE); // 清空删除请求缓冲区
}

// 打印写入请求
// 参数：
// - obj_pool: 对象池，用于获取对象信息
auto PrintWrite(ObjectPool &obj_pool) -> void {
  for (int i = 0; i < top[WRITE]; i++) {
    std::cout << buf[WRITE][i] + 1 << '\n'; // 打印写入对象的 ID（从 1 开始）
    auto obj = obj_pool.GetObjAt(buf[WRITE][i]); // 获取对象
    for (int j = 0; j < 3; j++) { // 遍历对象的每个副本
      std::cout << obj->idisk_[j] + 1 << ' '; // 打印副本所在的磁盘编号
      for (auto it : obj->tdisk_[j]) {
        std::cout << it + 1 << ' '; // 打印副本的块编号
      }
      std::cout << '\n';
    }
  }
  Clean(WRITE); // 清空写入请求缓冲区
}

// 打印读取请求
// 参数：
// - N: 磁盘数量
auto PrintRead(int N) -> void {
  for (int i = 0; i < N; i++) {
    auto &op = ops[i];
    if (op.empty() || op.back() == 'r' || op.back() == 'p') {
      op.push_back('#'); // 添加 '#' 表示操作结束
    }
    std::cout << op << '\n'; // 打印磁盘的操作记录
  }
  std::cout << top[READ] << '\n'; // 打印读取请求的数量
  for (int i = 0; i < top[READ]; i++) {
    std::cout << buf[READ][i] << '\n'; // 打印每个读取请求的 ID
  }
  Clean(READ); // 清空读取请求缓冲区
}

} // namespace printer

#endif