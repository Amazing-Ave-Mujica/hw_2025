#ifndef _DATA_HEADER
#define _DATA_HEADER
#include <cstddef>
#include <vector>

struct Data {
  int M, N;                          // NOLINT
  std::vector<std::vector<int>> vec; // NOLINT
  Data(int n_, int m_) : M(n_), N(m_), vec(M, std::vector<int>(N)) {}
  auto operator[](size_t x) -> auto&  { return vec[x]; }
  auto operator[](size_t x)  const { return vec[x]; }
};
#endif