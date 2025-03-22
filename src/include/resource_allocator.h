#pragma once
#include "config.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

class ResourceAllocator {
private:
  int m_;                                // 资源种类数
  int n_;                                // 容器数量
  int v_;                                // 每个容器的容量
  int l_;                                // 资源超额阈值
  std::vector<int> r_;                   // 每种资源的总量
  std::vector<std::vector<int>> best_x_; // 最优分配方案
  double best_penalty_;                  // 最优惩罚值

  // 惩罚系数
  std::vector<std::vector<double>> alpha_; // 资源混合惩罚系数
  std::vector<double> beta_;               // 超额部分惩罚系数

  // 随机数生成器
  std::mt19937 rng_;

  // **计算当前分配方案的惩罚值**
  auto ComputePenalty(const std::vector<std::vector<int>> &x) const -> double {
    double penalty = 0.0;

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
        penalty += beta_[i] * s_ij*s_ij;
      }
    }
    return penalty;
  }

  // **初始化可行解**
  auto InitializeSolution() -> std::vector<std::vector<int>> {
    std::vector<std::vector<int>> x(m_, std::vector<int>(n_, 0));
    std::vector<int> remaining = r_;

    // **随机打乱资源索引，提高公平性**
    std::vector<int> resource_indices(m_);
    for (int i = 0; i < m_; ++i) {
      resource_indices[i] = i;
    }
    std::shuffle(resource_indices.begin(), resource_indices.end(), rng_);

    // **轮盘式分配，尽量均匀**
    for (int i : resource_indices) {
      int allocated = 0; // 记录已分配的资源量
      while (remaining[i] > 0) {
        bool assigned = false;
        for (int j = 0; j < n_ && remaining[i] > 0; ++j) {
          if (allocated < v_) {
            int allocate_amount = std::min(remaining[i], 1);
            x[i][j] += allocate_amount;
            remaining[i] -= allocate_amount;
            allocated += allocate_amount;
            assigned = true;
          }
        }
        if (!assigned) {break;} // 如果无法继续分配，则停止
      }
    }

    return x;
  }

public:
  // **构造函数**
  ResourceAllocator(int m, int n, int v, int l, const std::vector<int> &r,
                    const std::vector<std::vector<double>> &alpha,
                    const std::vector<double> &beta)
      : m_(m), n_(n), v_(v), l_(l), r_(r), alpha_(alpha), beta_(beta),
        best_penalty_(std::numeric_limits<double>::max()),
        rng_(config::RANDOM_SEED) {}

  // **执行模拟退火优化**
  auto SimulatedAnnealing(double T, double coolingRate, int maxIter) -> void {
    std::vector<std::vector<int>> x = InitializeSolution();
    double e_cur = ComputePenalty(x);
    best_x_ = x;
    best_penalty_ = e_cur;

    std::uniform_int_distribution<int> dist_n(0, n_ - 1);
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);

    for (int iter = 0; iter < maxIter; ++iter) {
      // **随机选择两个不同容器 j1, j2**
      int j1 = dist_n(rng_);
      int j2 = dist_n(rng_);
      while (j1 == j2) {
        j2 = dist_n(rng_);
      }

      // **随机选择资源 i**
      int i = dist_m(rng_);

      // **交换 i 资源的部分数量**
      int delta = std::uniform_int_distribution<int>(
          0, std::min(x[i][j1], v_ - x[i][j2]))(rng_);
      std::vector<std::vector<int>> x_new = x;
      x_new[i][j1] -= delta;
      x_new[i][j2] += delta;

      // **计算新解的惩罚值**
      double e_new = ComputePenalty(x_new);

      // **接受新解的策略**
      if (e_new < e_cur ||
          std::exp(-(e_new - e_cur) / T) >
              std::uniform_real_distribution<double>(0.0, 1.0)(rng_)) {
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
      if (T < 1e-4) {
        std::cerr<<"epoch="<<iter<<'\n';
        break;
      }
    }
    /*调整最后的资源*/
    for(int i=0;i<n_;i++){
      int delta=0;
      for(int j=0;j<m_;j++){
        delta+=x[j][i];
      }
      delta-=v_;
      if(delta==0) {continue;}
      while(delta!=0){
        int j=dist_m(rng_);
        if(delta>0){
          int d=std::min(delta,x[j][i]);
          x[j][i]-=d;
          delta-=d;
        }else{
          x[j][i]-=delta;
          delta=0;
        }
      }
    }
    // for(int i=0;i<n_;i++){
    //   int delta=0;
    //   for(int j=0;j<m_;j++){
    //     delta+=x[j][i];
    //   }
    //   std::cerr<<delta<<'\n';
    // }
  }
  
  // **获取最优解**
  auto GetBestSolution() const -> std::vector<std::vector<int>> {
    std::cerr << "Minimum penalty: " << best_penalty_ << '\n';
    std::cerr << "Optimal allocation:\n";
    for (int i = 0; i < m_; ++i) {
      std::cerr << "Resource " << i + 1 << ": ";
      for (int j = 0; j < n_; ++j) {
        std::cerr << best_x_[i][j] << " ";
      }
      std::cerr << '\n';
    }
    return best_x_;
  }
};