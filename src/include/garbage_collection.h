/*
一个磁盘块的信息：pos,segment_tag,tag
返回值：pair<int,int>
*/
#pragma once
#include "config.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <vector>
template <typename T> struct Matrix : public std::vector<std::vector<T>> {
  using std::vector<std::vector<T>>::vector; // Inherit constructors
};

struct GarbageAllocator {
  Matrix<db> sim_;
  std::vector<std::tuple<db, int, int, int, int>> swaps_;
  const int m_;
  const int K_; // NOLINT
  GarbageAllocator(const std::vector<std::vector<db>> &alpha, int K)
      : sim_(alpha.size()), m_(alpha.size()), K_(K) {
    for (int i = 0; i < m_; i++) {
      sim_[i] = alpha[i];
      sim_[i].push_back(0);
    }
    // 预处理所有可能的操作，并按增量排序
    for (int r1 = 0; r1 < m_; r1++) {
      for (int r2 = 0; r2 < m_; r2++) {
        if (r1 == r2) {
          continue;
        }
        for (int c1 = 0; c1 < m_; c1++) {
          for (int c2 = 0; c2 < m_; c2++) {
            if (c1 == c2) {
              continue;
            }
            db delta =
                -sim_[r1][c1] - sim_[r2][c2] + sim_[r1][c2] + sim_[r2][c1];
            if (delta > 0) {
              swaps_.emplace_back(delta, r1, c1, r2, c2);
            }
          }
        }
      }
    }
    sort(swaps_.begin(), swaps_.end()); // 按增量降序排序
  }
  // 计算总相似度
  auto compute_similarity(const Matrix<int> &allo) -> db { // NOLINT
    db sum = 0;
    int m = allo.size();
    for (int i = 0; i < m; i++) {
      for (int j = 0; j < m + 1; j++) {
        sum += allo[i][j] * sim_[i][j];
      }
    }
    return sum;
  }

  // 进行最多 K 次操作，使总相似度最大
  //NOLINTNEXTLINE
  auto maximize_similarity(Matrix<int> &allo, Matrix<std::vector<int>> &pos)
      -> std::vector<std::pair<int, int>> { // NOLINT
    db max_similarity = compute_similarity(allo);
#ifdef ISCERR
    std::cerr << "init similarity:" << max_similarity << '\n';
#endif
    std::vector<std::pair<int, int>> ret;
    int i = K_;
    while (i > 0) {
      int lasti = i;
      for (auto it = swaps_.rbegin(); it != swaps_.rend(); it++) {
        auto [delta, r1, c1, r2, c2] = *it;
        while (allo[r1][c1] > 0 && allo[r2][c2] > 0 && i > 0) {
          allo[r1][c1]--;
          allo[r2][c2]--;
          allo[r1][c2]++;
          allo[r2][c1]++;
          ret.emplace_back(pos[r1][c1].back(), pos[r2][c2].back());
          pos[r2][c1].push_back(pos[r1][c1].back());
          pos[r1][c2].push_back(pos[r2][c2].back());
          pos[r1][c1].pop_back();
          pos[r2][c2].pop_back();
          max_similarity += delta;
          i--;
        }
      }
      if (i == lasti) {
        return ret;
      }
    }
#ifdef ISCERR
    std::cerr << "after similarity:" << max_similarity << '\n';
#endif

    return ret;
  }
  /*
  一个磁盘块的信息：pos,segment_tag,tag
  tag==m表示该位置为空
  返回值：pair<int,int>
  */
  //NOLINTNEXTLINE
  auto garbage_collection(const std::vector<std::tuple<int, int, int>> &info)
      -> std::vector<std::pair<int, int>> { // NOLINT
    Matrix<std::vector<int>> pos(m_, std::vector<std::vector<int>>(m_ + 1));
    Matrix<int> allo(m_, std::vector<int>(m_ + 1));
    for (auto [p, st, t] : info) {
      pos[st][t].push_back(p);
      allo[st][t]++;
    }
    return maximize_similarity(allo, pos);
  }
  //NOLINTNEXTLINE
  auto garbage_collection(Matrix<std::vector<int>> pos)
      -> std::vector<std::pair<int, int>> { // NOLINT
    Matrix<int> allo;
    for (int i = 0; i < m_; i++) {
      for (int j = 0; j <= m_; j++) {
        allo[i][j] = pos[i][j].size();
      }
    }
    return maximize_similarity(allo, pos);
  }
};
