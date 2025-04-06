#pragma once
#include "config.h"
#include "resource_allocator.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

class AnnealOptimizer {
private:
  int m_;                                // 资源种类数
  int n_;                                // 容器数量
  int v_;                                // 每个容器的容量
  int l_;                                // 资源超额阈值
  std::vector<int> r_;                   // 每种资源的总量
  std::vector<std::vector<int>> best_x_; // 最优分配方案
  db best_penalty_;                      // 最优惩罚值
  // 惩罚系数
  db beta_, gama_;                     // 超额部分惩罚系数
  std::vector<std::vector<db>> alpha_; // 资源混合惩罚系数

  // 随机数生成器
  std::mt19937 rng_;

  // **计算当前分配方案的惩罚值**
  auto ComputePenalty(const std::vector<std::vector<int>> &x) const -> db {
    db penalty = 0.0;

    for (int j = 0; j < n_; ++j) {
      // 计算混合惩罚项
      for (int i = 0; i < m_; ++i) {
        for (int k = i + 1; k < m_; ++k) {
          penalty += alpha_[i][k] * x[i][j] * x[k][j];
        }
      }
      // 计算超额惩罚项
      for (int i = 0; i < m_; ++i) {
        int s_ij = std::max(0, x[i][j] - l_);
        penalty += beta_ * s_ij * s_ij;
      }
    }
    return penalty;
  }

  // **初始化可行解**
  auto InitializeSolution() -> std::vector<std::vector<int>> {
    std::vector<std::vector<int>> x(m_, std::vector<int>(n_, 0));
    std::vector<int> remaining_r = r_; // 记录每种资源剩余可分配数量
    std::vector<int> remaining_v(n_, v_); // 记录每个容器剩余容量

    std::uniform_int_distribution<int> dist_m(0, m_ - 1);
    std::uniform_int_distribution<int> dist_n(0, n_ - 1);

    // **第一步：均匀分配资源，保证 r[i] 约束**
    for (int i = 0; i < m_; ++i) {
      while (remaining_r[i] > 0) {
        int j = dist_n(rng_);     // 随机选择容器
        if (remaining_v[j] > 0) { // 容器仍有可用容量
          int allocate_amount = std::min({remaining_r[i], remaining_v[j], 1});
          x[i][j] += allocate_amount;
          remaining_r[i] -= allocate_amount;
          remaining_v[j] -= allocate_amount;
        }
      }
    }
    // **第二步：调整，使得每个容器正好填满 v**
    for (int j = 0; j < n_; ++j) {
      int sum = 0;
      for (int i = 0; i < m_; ++i) {
        sum += x[i][j];
      }
      int diff = sum - v_;

      while (diff != 0) {
        int i = dist_m(rng_);
        if (diff > 0 && x[i][j] > 0) { // 超配，减少
          int adjust = std::min(diff, x[i][j]);
          x[i][j] -= adjust;
          diff -= adjust;
        } else if (diff < 0 && remaining_r[i] > 0) { // 少配，增加
          int adjust = std::min(-diff, remaining_r[i]);
          x[i][j] += adjust;
          remaining_r[i] -= adjust;
          diff += adjust;
        }
      }
    }
    // AdjustSolution(x);
    return x;
  }

public:
  // **构造函数**
  AnnealOptimizer(int m, int n, int v, int l, const std::vector<int> &r,
                  const std::vector<std::vector<db>> &alpha,
                  db beta = config::BETA_VALUE, db gama = config::GAMA_VALUE)
      : m_(m), n_(n), v_(v), l_(l), r_(r), alpha_(alpha), beta_(beta),
        gama_(gama), best_penalty_(std::numeric_limits<db>::max()),
        rng_(config::RANDOM_SEED) {}

  // **执行模拟退火优化**
  auto Solve(bool iscerr = false, db T = config::T,
             db coolingRate = config::COOLING_RATE,
             int maxIter = config::MAX_ITER) -> void {
    std::vector<std::vector<int>> x = InitializeSolution();
    db e_cur = ComputePenalty(x);
    best_x_ = x;
    best_penalty_ = e_cur;

    std::uniform_int_distribution<int> dist_n(0, n_ - 1);
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);

    for (int iter = 0; iter < maxIter; ++iter) {
      std::vector<std::vector<int>> x_new = x;
      int i1 = dist_m(rng_);
      int i2 = dist_m(rng_);
      int j1 = dist_n(rng_);
      int j2 = dist_n(rng_);
      std::uniform_int_distribution<int> dist_r(0,
                                                std::min(x[i2][j1], x[i1][j2]));
      int delta = dist_r(rng_);
      x_new[i1][j1] += delta;
      x_new[i2][j1] -= delta;
      x_new[i2][j2] += delta;
      x_new[i1][j2] -= delta;

      // **计算新解的惩罚值**
      db e_new = ComputePenalty(x_new);

      // **接受新解的策略**
      if (e_new < e_cur ||
          std::exp(-(e_new - e_cur) / T) >
              std::uniform_real_distribution<db>(0.0, 1.0)(rng_)) {
        x = x_new;
        e_cur = e_new;
        if (e_new < best_penalty_) {
          best_penalty_ = e_new;
          best_x_ = x;
        }
      }
      // **温度衰减**
      T *= coolingRate;

      // **终止条件**
      if (T < config::EPS_T) {
#ifdef ISCERR
        { std::cerr << "epoch=" << iter << '\n'; }
#endif
        break;
      }
    }
    //**调整最后的资源**
    // AdjustSolution(best_x_);
  }

  // **获取最优解**
  auto
  GetBestSolution(bool iscerr = false) const -> std::vector<std::vector<int>> {
#ifdef ISCERR
    std::vector<int> c(n_, 0), r(m_, 0); // NOLINT
    std::cerr << "Minimum penalty: " << best_penalty_ << '\n';
    std::cerr << "Optimal allocation:\n";
    for (int i = 0; i < m_; ++i) {
      std::cerr << "Resource " << i + 1 << ": ";
      for (int j = 0; j < n_; ++j) {
        std::cerr << best_x_[i][j] << " ";
        c[j] += best_x_[i][j];
        r[i] += best_x_[i][j];
      }
      std::cerr << '\n';
    }
    for (int i = 0; i < n_; i++) {
      assert(c[i] == v_);
    }
    for (int i = 0; i < m_; i++) {
      assert(r[i] == r_[i]);
    }
    for (int i = 0; i < m_; i++) {
      for (int j = 0; j < m_; j++) {
        std::cerr << alpha_[i][j] << ' ';
      }
      std::cerr << '\n';
    }
#endif
    return best_x_;
  }
};
using ResourceAllocator = AnnealOptimizer;
