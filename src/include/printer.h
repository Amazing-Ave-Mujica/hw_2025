
#include <cstdio>
#ifndef _PRINTER_H
#define _PRINTER_H
#include "object.h"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>

namespace printer {
int buf[3][1 << 20], top[3] = {0};
std::string ops[10];

// NOLINTNEXTLINE
enum { DELETE = 0, WRITE, READ };

void Clean(int idx) {
  std::cout.flush();
  top[idx] = 0;
  for (auto &op : ops) {
    op.clear();
  }
}

void AddReadRequest(int RequestID) { buf[READ][top[READ]++] = RequestID; }

void ReadAddPass(size_t DiskNum, int cnt) {
  for (int i = 0; i < cnt; i++) {
    ops[DiskNum].push_back('p');
  }
}
void ReadAddRead(size_t DiskNum, int cnt) {
  for (int i = 0; i < cnt; i++) {
    ops[DiskNum].push_back('r');
  }
}
void ReadSetJump(size_t DiskNum, size_t DiskBlockID) {
  ops[DiskNum] = "j " + std::to_string(DiskBlockID);
}

void AddInsertedObject(int ObjectID) { buf[WRITE][top[WRITE]++] = ObjectID; }

void AddDeletedRequest(int RequestID) {
  buf[DELETE][top[DELETE]++] = RequestID;
}
auto PrintDelete() -> void {
  std::cout << top[DELETE] << '\n';
  for (int i = 0; i < top[DELETE]; i++) {
    std::cout << buf[DELETE][i] << '\n';
  }
  Clean(DELETE);
}

auto PrintWrite(ObjectPool &obj_pool) -> void {
  for (int i = 0; i < top[WRITE]; i++) {
    std::cout << buf[WRITE][i] + 1 << '\n';
    auto obj = obj_pool.GetObjAt(buf[WRITE][i]);
    for (int j = 0; j < 3; j++) {

      //   std::cerr << "WRITE: DISKNUM " << obj->idisk_[j] + 1 << '\n';

      std::cout << obj->idisk_[j] + 1 << ' ';
      for (auto it : obj->tdisk_[j]) {
        std::cout << it + 1 << ' ';
      }
      std::cout << '\n';
    }
  }
  Clean(WRITE);
}

auto PrintRead(int N) -> void {
  for (int i = 0; i < N; i++) {
    auto &op = ops[i];
    if (op.empty() || op.back() == 'r' || op.back() == 'p') {
      op.push_back('#');
    }
    std::cout << op << '\n';
  }
  std::cout << top[READ] << '\n';
  for (int i = 0; i < top[READ]; i++) {
    std::cout << buf[READ][i] << '\n';
  }
  Clean(READ);
}
} // namespace printer

#endif