#pragma once

#include "config.h"
#include "data.h"
#include "resource_allocator.h"
#include "tsp.h"
#include <iostream>
#include <numeric>
#include <vector>

struct Spearman{
  // 结构体用于存储值及其原始索引
struct RankData {
  db value;//NOLINT
  int index;//NOLINT
};

// 比较函数用于排序
static auto compare(RankData a, RankData b) -> bool { // NOLINT
  return a.value < b.value;
}

// 计算秩
auto getRanks(const  std::vector<db>& data) ->std::vector<db>{//NOLINT
  int n = data.size();
  std::vector<RankData> rankData(n);//NOLINT
  
  // 记录原始值和索引
  for (int i = 0; i < n; i++) {
      rankData[i] = {data[i], i};
  }

  // 按值排序
  sort(rankData.begin(), rankData.end(), compare);//NOLINT

  // 计算秩
  std::vector<db> ranks(n);
  int i = 0;
  while (i < n) {
      int j = i;
      db sumRank = 0.0;//NOLINT
      while (j < n && rankData[j].value == rankData[i].value) {
          sumRank += (j + 1);  // 1-based rank
          j++;
      }
      db avgRank = sumRank / (j - i);//NOLINT
      for (int k = i; k < j; k++) {
          ranks[rankData[k].index] = avgRank;
      }
      i = j;
  }

  return ranks;
}

// 计算斯皮尔曼秩相关系数
auto spearmanCorrelation(const std::vector<db>& x, const std::vector<db>& y)->db {//NOLINT
  int n = x.size();
  if (n != y.size() || n == 0) {
      std::cerr << "Error: Input vectors must have the same non-zero size.\n";
      return NAN;
  }

  // 计算秩
  std::vector<db> rankX = getRanks(x);//NOLINT
  std::vector<db> rankY = getRanks(y);//NOLINT

  // 计算秩差的平方和
  db dSquaredSum = 0.0;//NOLINT
  for (int i = 0; i < n; i++) {
      db d = rankX[i] - rankY[i];
      dSquaredSum += d * d;
  }

  // 计算斯皮尔曼相关系数
  db spearman = 1.0 - (6.0 * dSquaredSum) / (n * (n * n - 1));//NOLINT
  return spearman;
}
};
static constexpr int TIME_SLICE_DIVISOR =
    config::TIME_SLICE_DIVISOR; // 常量替代魔法数字

auto InitResourceAllocator(int t, int m, int n, int v, int g,
                           const Data &delete_data, const Data &write_data,
                           const Data &read_data)
    -> std::pair<std::vector<std::vector<int>>, std::vector<std::vector<db>>> {
  // 初始化时间片数据
  std::vector<std::vector<int>> timeslice_data(
      m, std::vector<int>(((t - 1) / TIME_SLICE_DIVISOR) + 1, 0));
  // 初始化资源混合惩罚系数矩阵
  std::vector<std::vector<db>> alpha(m, std::vector<db>(m, 0.0));
  // 初始化最大分配值和资源数组
  std::vector<int> max_allocate(m, 0), resource(m, 0); // NOLINT

  // 处理删除数据
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      timeslice_data[i][j] -= delete_data[i][j]; // 减去删除数据
    }
  }

  // 处理写入数据
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      timeslice_data[i][j] += write_data[i][j]; // 加上写入数据
    }
  }

  // 计算 timeslice_data 的累积和和 max_allocate
  for (int i = 0; i < m; i++) {
    for (int j = 1; j < (t - 1) / TIME_SLICE_DIVISOR + 1; j++) {
      timeslice_data[i][j] += timeslice_data[i][j - 1]; // 累积和
    }
    max_allocate[i] = *std::max_element(timeslice_data[i].begin(),
                                        timeslice_data[i].end()); // 最大分配值
  }

  // 计算资源分配
  if constexpr (config::WritePolicy() == config::compact) {
    for (int i = 0; i < m; i++) {
      if (i < m - 1) {
        resource[i] = max_allocate[i] * (n * (v / 3)) /
                      std::accumulate(max_allocate.begin(), max_allocate.end(),
                                      0); // 按比例分配资源
      } else {
        resource[i] =
            n * (v / 3) - std::accumulate(resource.begin(), resource.end(),
                                          0); // 剩余资源分配给最后一个标签
      }
    }
  } else {
    for (int i = 0; i < m; i++) {
      if (i < m - 1) {
        resource[i] = max_allocate[i] * (n * v) /
                      std::accumulate(max_allocate.begin(), max_allocate.end(),
                                      0); // 按比例分配资源
      } else {
        resource[i] = n * (v)-std::accumulate(resource.begin(), resource.end(),
                                              0); // 剩余资源分配给最后一个标签
      }
    }
  }

  // 计算 alpha 矩阵
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < m; j++) {
      std::vector<db> read_data_i(read_data[i].size());
      std::vector<db> read_data_j(read_data[j].size());
      for(int t=0;t<read_data_i.size();t++){
        read_data_i[t]=read_data[i][t];
        read_data_j[t]=read_data[j][t];
      }
      alpha[i][j] = Spearman().spearmanCorrelation(read_data_i, read_data_j);
    }
  }

  // 初始化资源分配器并求解
  if constexpr (config::WritePolicy() == config::compact) {
    ResourceAllocator ra(m, n, v / 3, v*2 / (3*m), resource, alpha);
    ra.Solve(ISCERR);
    return {ra.GetBestSolution(ISCERR), alpha};
  }
  ResourceAllocator ra(m, n, v, v / m, resource, alpha);
  ra.Solve(ISCERR);
  return {ra.GetBestSolution(ISCERR), alpha};
}

auto InitTSP(int n, int m, const std::vector<std::vector<db>> &alpha,
             const std::vector<std::vector<int>> &solution)
    -> std::vector<std::vector<int>> {
  std::vector<std::vector<int>> ans;
#ifdef USINGTSP
  for (int _ = 0; _ < n; _++) {
    std::vector<std::vector<db>> dist(m, std::vector<db>(m, 0));
    for (int i = 0; i < m; i++) {
      for (int j = 0; j < m; j++) {
        dist[i][j] = alpha[i][j] * solution[i][_] * solution[j][_];
      }
    }
    auto tsp = TSP(m, dist);
#ifdef ISCERR
    for (auto x : tsp) {
      std::cerr << x << ' ';
    }
    std::cerr << '\n';
#endif
    ans.push_back(tsp);
  }
  return ans;
#else
  std::vector<int> tmp(m);
  iota(tmp.begin(), tmp.end(), 0);
  ans.assign(n, tmp);
  return ans;
#endif
}
//1012