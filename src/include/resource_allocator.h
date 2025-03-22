#pragma once
#include "config.h"
#include "resource_allocator.h"
#include <algorithm>
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
  db best_penalty_;                  // 最优惩罚值
  // 惩罚系数
  db beta_,gama_;                             // 超额部分惩罚系数
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
        penalty += beta_ * s_ij*s_ij;
      }
    }
    //正则项，保证每个磁盘的资源分配均匀
    for(int i=0;i<n_;i++){
      int count_in_disk=0;
      for(int j=0;j<m_;j++){
        count_in_disk+=x[j][i];
      }
      penalty+=gama_*(count_in_disk-v_)*(count_in_disk-v_);
    }
    //正则项，保证每种资源的分配均匀
    for(int j=0;j<m_;j++){
      int count_in_resource=0;
      for(int i=0;i<n_;i++){
        count_in_resource+=x[j][i];
      }
      penalty+=gama_*(count_in_resource-r_[j])*(count_in_resource-r_[j]);
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
  // **调整资源分配，使得每个容器的资源分配为v**
  auto AdjustSolution()->void{
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);
    for(int i=0;i<n_;i++){
      int delta=-v_;
      for(int j=0;j<m_;j++){
        delta+=best_x_[j][i];
      }
      while(delta!=0){
        int j=dist_m(rng_);
        if(delta>0){
          int d=std::min(delta,best_x_[j][i]);
          best_x_[j][i]-=d;
          delta-=d;
        }else{
          best_x_[j][i]-=delta;
          delta=0;
        }
      }
    }
  }
public:
  // **构造函数**
  AnnealOptimizer(int m, int n, int v, int l, const std::vector<int> &r,
                    const std::vector<std::vector<db>> &alpha,
                    db beta=config::BETA_VALUE,db gama=config::GAMA_VALUE)
      : m_(m), n_(n), v_(v), l_(l), r_(r), alpha_(alpha), beta_(beta),gama_(gama),
        best_penalty_(std::numeric_limits<db>::max()),
        rng_(config::RANDOM_SEED) {}

  // **执行模拟退火优化**
  auto Solve(bool iscerr=false,db T=config::T, db coolingRate=config::COOLING_RATE, int maxIter=config::MAX_ITER) -> void {
    std::vector<std::vector<int>> x = InitializeSolution();
    db e_cur = ComputePenalty(x);
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
        if(iscerr){std::cerr<<"epoch="<<iter<<'\n';}
        break;
      }
    }
    //**调整最后的资源**
    AdjustSolution();
  }
  
  // **获取最优解**
  auto GetBestSolution(bool iscerr=false) const -> std::vector<std::vector<int>> {
    if(iscerr){
      std::cerr << "Minimum penalty: " << best_penalty_ << '\n';
      std::cerr << "Optimal allocation:\n";
      for (int i = 0; i < m_; ++i) {
        std::cerr << "Resource " << i + 1 << ": ";
        for (int j = 0; j < n_; ++j) {
          std::cerr << best_x_[i][j] << " ";
        }
        std::cerr << '\n';
      }
    }
    return best_x_;
  }
};


//----------------------------------------------------------------
// *********************遗传算法优化器**************************
//----------------------------------------------------------------
constexpr int K_POP_SIZE = config::K_POP_SIZE;       // 种群大小
constexpr int K_MAX_GEN = config::K_MAX_GEN;        // 最大迭代次数
constexpr db K_CROSS_RATE = config::K_CROSS_RATE; // 交叉概率
constexpr db K_MUTATE_RATE = config::K_MUTATE_RATE; // 变异概率

struct Individual {
  std::vector<std::vector<int>> allocation_; // 资源分配方案 (m × n 矩阵)
  db fitness_;                           // 适应度（即总惩罚值）
};

class GeneticOptimizer {
private:
  int m_, n_, v_, l_;
  db beta_,gama_;
  std::vector<int> r_;
  std::vector<std::vector<db>> alpha_;
  std::vector<Individual> population_;
  // 随机数生成器
  std::mt19937 rng_;

  // 计算惩罚值（适应度）
  auto ComputePenalty(const std::vector<std::vector<int>> &x) const -> db {
    db penalty = 0.0;

    // 资源混合惩罚
    for (int j = 0; j < n_; ++j) {
      for (int i = 0; i < m_; ++i) {
        for (int k = i + 1; k < m_; ++k) {
          penalty += alpha_[i][k] * x[i][j] * x[k][j];
        }
      }
    }

    // 超额部分惩罚
    for (int j = 0; j < n_; ++j) {
      for (int i = 0; i < m_; ++i) {
        int excess = std::max(0, x[i][j] - l_);
        penalty += beta_ * excess*excess;
      }
    }
    //正则项，保证每个磁盘的资源分配均匀
    for(int i=0;i<n_;i++){
      int count_in_disk=0;
      for(int j=0;j<m_;j++){
        count_in_disk+=x[j][i];
      }
      penalty+=gama_*(count_in_disk-v_)*(count_in_disk-v_);
    }
    //正则项，保证每种资源的分配均匀
    for(int j=0;j<m_;j++){
      int count_in_resource=0;
      for(int i=0;i<n_;i++){
        count_in_resource+=x[j][i];
      }
      penalty+=gama_*(count_in_resource-r_[j])*(count_in_resource-r_[j]);
    }
    return penalty;
  }

  // 初始化种群
  auto InitializePopulation() -> void {
    for (int p = 0; p < K_POP_SIZE; ++p) {
      Individual indiv;
      indiv.allocation_ = std::vector<std::vector<int>>(m_, std::vector<int>(n_, 0));
      std::vector<int> remaining = r_; // 剩余资源

      // 随机分配资源
      for (int i = 0; i < m_; ++i) {
        std::vector<int> indices(n_);
        //打乱顺序
        iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng_);
        for (int _ = 0; _ < n_; ++_) {
          int j = indices[_];
          if (remaining[i] > 0) {
            std::uniform_int_distribution<int> dist(0, std::min(remaining[i], v_));
            int allocation = dist(rng_);
            indiv.allocation_[i][j] = allocation;
            remaining[i] -= allocation;
          }
        }
      }

      indiv.fitness_ = ComputePenalty(indiv.allocation_);
      population_.push_back(indiv);
    }
  }

  // 选择操作（锦标赛选择）
  auto TournamentSelection() -> Individual {
    std::uniform_int_distribution<int> dist(0, K_POP_SIZE - 1);

    Individual best = population_[dist(rng_)];
    for (int i = 0; i < 3; ++i) {
      Individual competitor = population_[dist(rng_)];
      if (competitor.fitness_ < best.fitness_) {
        best = competitor;
      }
    }
    return best;
  }

  // 交叉操作（单点交叉）
  auto Crossover(const Individual &parent1, const Individual &parent2) -> Individual {
    std::uniform_int_distribution<int> dist(0, m_ - 1);
    std::uniform_int_distribution<int>dist_cross(0,100);
    Individual offspring = parent1;

    if (static_cast<db>(dist_cross(rng_)) / 100 <  K_CROSS_RATE) {
      int crossover_point = dist(rng_);

      for (int i = crossover_point; i < m_; ++i) {
        offspring.allocation_[i] = parent2.allocation_[i];
      }
    }

    offspring.fitness_ = ComputePenalty(offspring.allocation_);
    return offspring;
  }

  // 变异操作
  auto Mutate(Individual &indiv) -> void {
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);
    std::uniform_int_distribution<int> dist_n(0, n_ - 1);
    std::uniform_int_distribution<int>dist_mutate(0,100);

    if (static_cast<db>(dist_mutate(rng_)) / 100 < K_MUTATE_RATE) {
      int i = dist_m(rng_);
      int j = dist_n(rng_);

      std::uniform_int_distribution<int> dist_v(-v_/2, v_/2);
      int delta = dist_v(rng_);
      int new_value = std::max(0, indiv.allocation_[i][j] + delta);
      indiv.allocation_[i][j] = std::min(new_value, v_);
    }

    indiv.fitness_ = ComputePenalty(indiv.allocation_);
  }
  // **调整资源分配，使得每个容器的资源分配为v**
  auto AdjustSolution(std::vector<std::vector<int>>&best_x_)->void{
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);
    for(int i=0;i<n_;i++){
      int delta=-v_;
      for(int j=0;j<m_;j++){
        delta+=best_x_[j][i];
      }
      while(delta!=0){
        int j=dist_m(rng_);
        if(delta>0){
          int d=std::min(delta,best_x_[j][i]);
          best_x_[j][i]-=d;
          delta-=d;
        }else{
          best_x_[j][i]-=delta;
          delta=0;
        }
      }
    }
  }
public:
  GeneticOptimizer(int m, int n, int v, int l, std::vector<int> r,
                   std::vector<std::vector<db>> alpha, db beta=config::BETA_VALUE,db gama=config::GAMA_VALUE)
      : m_(m), n_(n), v_(v), l_(l), r_(std::move(r)), alpha_(std::move(alpha)),
        beta_(beta),gama_(gama),rng_(config::RANDOM_SEED) {}

  auto Solve(bool cerr=false) -> void {
    InitializePopulation();

    for (int gen = 0; gen < K_MAX_GEN; ++gen) {
      std::vector<Individual> new_population;

      // 选择 + 交叉 + 变异
      for (int i = 0; i < K_POP_SIZE; ++i) {
        Individual parent1 = TournamentSelection();
        Individual parent2 = TournamentSelection();
        Individual offspring = Crossover(parent1, parent2);
        Mutate(offspring);
        new_population.push_back(offspring);
      }

      // 更新种群
      population_ = std::move(new_population);

      // 输出当前最优解
      if (gen % 100 == 0) {
        db best_fitness = population_[0].fitness_;
        for (const auto &indiv : population_) {
          best_fitness = std::min(best_fitness, indiv.fitness_);
        }
        if(cerr){std::cerr << "Generation " << gen << " Best Fitness: " << best_fitness << '\n';}
      }
    }
  }

  auto GetBestSolution(bool iscerr=false) -> std::vector<std::vector<int>> {
    Individual best = population_[0];
    for (const auto &indiv : population_) {
      if (indiv.fitness_ < best.fitness_) {
        best = indiv;
      }
    }
    AdjustSolution(best.allocation_);
    if(iscerr){
      std::cerr << "Optimal Solution (Penalty: " << best.fitness_ << "):\n";
      for (int i = 0; i < m_; ++i) {
        std::cerr << "Resource " << i + 1 << ": ";
        for (int j = 0; j < n_; ++j) {
          std::cerr << best.allocation_[i][j] << " ";
        }
        std::cerr << '\n';
      }
    }
    return best.allocation_;
  }
};
using ResourceAllocator=AnnealOptimizer;
// using ResourceAllocator=GeneticOptimizer;
/*
baseline:AnnealOptimizer Minimum penalty: 1.55361e+08
*/