#ifndef _DATA_HEADER
#define _DATA_HEADER

#include <cstddef>
#include <vector>

// 数据结构体，用于存储二维数据
struct Data {
  int M, N;                          // 矩阵的行数 (M) 和列数 (N) NOLINT
  std::vector<std::vector<int>> vec; // 二维向量，用于存储数据 NOLINT

  // 构造函数
  // 参数：
  // - n_: 矩阵的行数
  // - m_: 矩阵的列数
  // 初始化二维向量 vec，大小为 M x N
  Data(int n_, int m_) : M(n_), N(m_), vec(M, std::vector<int>(N)) {}

  // 重载下标运算符，用于访问二维向量的行
  // 参数：
  // - x: 行索引
  // 返回值：引用类型，表示第 x 行的数据
  auto operator[](size_t x) -> auto & { return vec[x]; }

  // 重载下标运算符（常量版本），用于访问二维向量的行
  // 参数：
  // - x: 行索引
  // 返回值：常量引用类型，表示第 x 行的数据
  auto operator[](size_t x) const { return vec[x]; }
};

#endif