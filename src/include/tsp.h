#pragma once

#include "config.h"
#include <algorithm>
#include <climits>
#include <iostream>
#include <vector>

// TSP 动态规划求解 + 路径回溯
auto TSP(int n, const std::vector<std::vector<db>> &dist) -> std::vector<int> {
  int full_mask = (1 << n) - 1; // 终态：所有城市都访问过
  std::vector<std::vector<db>> dp(
      1 << n, std::vector<db>(
                  n, -config::INF)); // dp[S][i]: 访问 S，最后停在 i 的最短路径
  std::vector<std::vector<int>> parent(1 << n,
                                       std::vector<int>(n, -1)); // 记录转移路径
  for(int i=0;i<n;i++){
    dp[1<<i][i] = 0; // 初始状态：仅访问了起点 0
  }

  // 动态规划求解
  for (int s = 1; s < (1 << n); s++) {
    for (int i = 0; i < n; i++) {
      if ((s & (1 << i)) == 0) {
        continue;
      } // i 不在 s 中，跳过
      for (int j = 0; j < n; j++) {
        if ((s & (1 << j)) > 0) {
          continue;
        } // j 已访问 或 i==j 跳过
        int new_s = (s | (1 << j)); // 新状态，加入 j
        db res = dp[s][i] + dist[i][j];
        if (dp[new_s][j] < res) {
          dp[new_s][j] = res;
          parent[new_s][j] = i; // 记录 j 的前驱是 i
        }
      }
    }
  }

  // 找到最优解，并回溯路径
  db max_fitness = -config::INF;
  int last_city = -1;
  for (int j = 1; j < n; j++) {
    if (dp[full_mask][j] > max_fitness) {
      max_fitness = dp[full_mask][j] ;
      last_city = j;
    }
  }

  // 回溯路径
  std::vector<int> path;
  int state = full_mask;
  while (last_city != -1) {
    path.push_back(last_city);
    int prev_city = parent[state][last_city];
    state ^= (1 << last_city); // 移除 last_city
    last_city = prev_city;
  }
  // path.push_back(0); // 最后回到起点
  std::reverse(path.begin(), path.end()); // 逆序得到正确路径
  return path;
}
