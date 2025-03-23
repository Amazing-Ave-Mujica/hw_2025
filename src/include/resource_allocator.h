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
    //正则项，保证每种资源分配靠近r[i]  
    for(int i=0;i<m_;i++){
      int count_in_resource=0;
      for(int j=0;j<n_;j++){
        count_in_resource+=x[i][j];
      }
      int excess = count_in_resource - r_[i];
      penalty+=gama_*excess*excess;
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
    AdjustSolution(x);
    return x;
  }
  // **调整资源分配，使得每个容器的资源分配为v**
  auto AdjustSolution(std::vector<std::vector<int>>&x)->void{
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);
    for(int i=0;i<n_;i++){
      int delta=-v_;
      for(int j=0;j<m_;j++){
        delta+=x[j][i];
      }
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
      // **随机选择两个种不同资源 i1, i2**
      int i1 = dist_m(rng_);
      int i2 = dist_m(rng_);
      while (i1 == i2) {
        i2 = dist_n(rng_);
      }

      // **随机选择容器 j**
      int j = dist_n(rng_);

      // **交换 j 容器的部分数量**
      int delta = std::uniform_int_distribution<int>(
          0, std::min(x[i1][j], x[i2][j]))(rng_);
      std::vector<std::vector<int>> x_new = x;
      x_new[i1][j] -= delta;
      x_new[i2][j] += delta;

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
    // AdjustSolution(best_x_);
  }
  
  // **获取最优解**
  auto GetBestSolution(bool iscerr=false) const -> std::vector<std::vector<int>> {
    if(iscerr){
      std::vector<int>c(n_,0);
      std::cerr << "Minimum penalty: " << best_penalty_ << '\n';
      std::cerr << "Optimal allocation:\n";
      for (int i = 0; i < m_; ++i) {
        std::cerr << "Resource " << i + 1 << ": ";
        for (int j = 0; j < n_; ++j) {
          std::cerr << best_x_[i][j] << " ";
          c[j]+=best_x_[i][j];
        }
        std::cerr << '\n';
      }
      for(int i=0;i<n_;i++){
        std::cerr<<c[i]<<' ';
      }
      std::cerr<<'\n';
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
constexpr int ELITE_NUM=config::ELITE_NUM;//精英个体数量

struct Individual {
  std::vector<std::vector<int>> allocation_; // 资源分配方案 (m × n 矩阵)
  db fitness_;                           // 适应度（即总惩罚值）
};

class GeneticOptimizer {
private:
  const int m_, n_, v_, l_;
  const db beta_,gama_;
  std::vector<int> r_;
  const std::vector<std::vector<db>> alpha_;
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
        // int excess = x[i][j]-l_;
        int excess = std::max(0,x[i][j] - l_);
        penalty += beta_ * excess*excess;
      }
    }
    /*正则项，保证每个磁盘的资源分配均匀
    for(int i=0;i<n_;i++){
      int count_in_disk=0;
      for(int j=0;j<m_;j++){
        count_in_disk+=x[j][i];
      }
      int excess = count_in_disk - v_;
      penalty+=gama_*excess*excess;
    }
      */
    //正则项，保证每种资源分配靠近r[i]  
    for(int i=0;i<m_;i++){
      int count_in_resource=0;
      for(int j=0;j<n_;j++){
        count_in_resource+=x[i][j];
      }
      int excess = count_in_resource - r_[i];
      penalty+=gama_*excess*excess;
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
      AdjustSolution(indiv.allocation_);
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
  // 选择操作（轮盘赌选择）
  auto RouletteWheelSelection() -> Individual {
    // 计算总适应度的倒数（适应度越小越好）
    db total_fitness = 0.0;
    for (const auto &indiv : population_) {
        total_fitness += 1.0 / indiv.fitness_; // 使用适应度的倒数作为概率权重
    }

    // 生成一个 [0, total_fitness] 范围内的随机数
    std::uniform_real_distribution<db> dist(0.0, total_fitness);
    db rand_value = dist(rng_);

    // 遍历种群，找到对应的个体
    db cumulative_fitness = 0.0;
    for (const auto &indiv : population_) {
        cumulative_fitness += 1.0 / indiv.fitness_;
        if (cumulative_fitness >= rand_value) {
            return indiv; // 返回被选中的个体
        }
    }

    // 如果没有找到（理论上不会发生），返回最后一个个体
    return population_.back();
  }
  // 交叉操作
  auto Crossover(const Individual &parent1, const Individual &parent2) -> Individual {
    std::uniform_real_distribution<db>dist(0.0,1.0);
    Individual offspring = parent1;

    for(int i=0;i<n_;i++){
      db crossover_point = dist(rng_);
      if(crossover_point<K_CROSS_RATE){
        for(int j=0;j<m_;j++){
          offspring.allocation_[j][i]=parent2.allocation_[j][i];
        }
      }
    }

    offspring.fitness_ = ComputePenalty(offspring.allocation_);
    return offspring;
  }

  // 变异操作
  auto Mutate(Individual &indiv) -> void {
    std::uniform_int_distribution<int> dist_m(0, m_ - 1);
    std::uniform_int_distribution<int> dist_n(0, n_ - 1);
    std::uniform_real_distribution<db>dist_mutate(0.0,1.0);

    if (dist_mutate(rng_) < K_MUTATE_RATE) {
      int i1 = dist_m(rng_);
      int i2 = dist_m(rng_);
      int j = dist_n(rng_);
      while(i1==i2){
        i2=dist_m(rng_);
      }

      std::uniform_int_distribution<int> dist_v(0,indiv.allocation_[i2][j]);
      int delta = dist_v(rng_);
      indiv.allocation_[i1][j] += delta;
      indiv.allocation_[i2][j] -= delta; 
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
                   std::vector<std::vector<db>> alpha, db beta=config::BETA_VALUE,db gama=config::GAMA_VALUE,
                   int seed = config::RANDOM_SEED)
      : m_(m), n_(n), v_(v), l_(l), r_(std::move(r)), alpha_(std::move(alpha)),
        beta_(beta),gama_(gama),rng_(seed) {}

  auto Solve(bool cerr = false) -> void {
    InitializePopulation();
    assert(ELITE_NUM<K_POP_SIZE);
    for (int gen = 0; gen < K_MAX_GEN; ++gen) {
      std::vector<Individual> new_population;

      // **精英主义：保留当前种群中适应度最高的三个个体**
      std::sort(population_.begin(), population_.end(),
                [](const Individual &a, const Individual &b) {
                  return a.fitness_ < b.fitness_; // 按适应度升序排序
                });
      new_population.reserve(K_POP_SIZE);
      for (int i = 0; i < ELITE_NUM; ++i) {
        new_population.push_back(population_[i]); // 保留前三个个体
      }
  
      // **选择 + 交叉 + 变异**
      for (int i = ELITE_NUM; i < K_POP_SIZE; ++i) { // 从第4个位置开始填充新种群
        Individual parent1=RouletteWheelSelection();
        Individual parent2=RouletteWheelSelection();
        Individual offspring = Crossover(parent1, parent2);
        Mutate(offspring);
        new_population.push_back(offspring);
      }

      // **更新种群**
      population_ = std::move(new_population);

      // **输出当前最优解**
      if (gen % 100 == 0) {
        db best_fitness = population_[0].fitness_;
        for (const auto &indiv : population_) {
          best_fitness = std::min(best_fitness, indiv.fitness_);
        }
        if (cerr) {
          std::cerr << "Generation " << gen << " Best Fitness: " << best_fitness << '\n';
        }
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
    // AdjustSolution(best.allocation_);
    if(iscerr){
      std::vector<int>c(n_,0);;
      std::cerr << "Optimal Solution (Penalty: " << best.fitness_ << "):\n";
      for (int i = 0; i < m_; ++i) {
        std::cerr << "Resource " << i + 1 << ": ";
        for (int j = 0; j < n_; ++j) {
          std::cerr << best.allocation_[i][j] << " ";
          c[j]+=best.allocation_[i][j];
        }
        std::cerr << '\n';
      }
      for(int i=0;i<n_;i++){
        std::cerr<<c[i]<<' ';
      }
      std::cerr<<'\n';
    }
    return best.allocation_;
  }
};
using ResourceAllocator=AnnealOptimizer;
// using ResourceAllocator=GeneticOptimizer;
/*
baseline:AnnealOptimizer 
gama=10 Minimum penalty: 1.55361e+08
gama=1 Minimum penalty: 1.50868e+08

2.28e8
*/