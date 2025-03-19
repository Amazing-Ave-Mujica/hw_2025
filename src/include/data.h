#include <cstddef>
#include <vector>

struct Data {
  int n, m;                          // NOLINT
  std::vector<std::vector<int>> vec; // NOLINT
  Data(int n_, int m_) : n(n_), m(m_), vec(n, std::vector<int>(m)) {}
  auto operator[](size_t x) -> auto& { return vec[x]; }
};