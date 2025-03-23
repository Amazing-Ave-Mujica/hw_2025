#pragma once

#include "config.h"
#include "data.h"
#include "resource_allocator.h"
#include "tsp.h"
#include <iostream>
#include <vector>

static constexpr int TIME_SLICE_DIVISOR = config::TIME_SLICE_DIVISOR; // 常量替代魔法数字

auto InitResourceAllocator(int t, int m, int n, int v, int g, Data delete_data, Data write_data, Data read_data) -> 
    std::pair<std::vector<std::vector<int>>,std::vector<std::vector<db>>> {
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
        max_allocate[i] = *std::max_element(timeslice_data[i].begin(), timeslice_data[i].end()); // 最大分配值
    }

    // 计算资源分配
    for (int i = 0; i < m; i++) {
        if (i < m - 1) {
            resource[i] = static_cast<int>(
                1.0 * max_allocate[i] * (n * v) /
                std::accumulate(max_allocate.begin(), max_allocate.end(), 0)); // 按比例分配资源
        } else {
            resource[i] = n * v - std::accumulate(resource.begin(), resource.end(), 0); // 剩余资源分配给最后一个标签
        }
    }

    // 计算 alpha 矩阵
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) {
            int sumi = 0, sumj = 0; // NOLINT
            for (int k = 0; k < (t - 1) / TIME_SLICE_DIVISOR + 1; k++) {
                alpha[i][j] += std::min(read_data[i][k], read_data[j][k]); // 计算混合惩罚项
                sumi += read_data[i][k];
                sumj += read_data[j][k];
            }
            int sum = std::min(sumi, sumj);
            if (sum > 0) { // 避免除以零
                alpha[i][j] /= sum;
            }
        }
    }

    // 初始化资源分配器并求解
    ResourceAllocator ra(m, n, v, v / m, resource, alpha);
    ra.Solve(ISCERR);
    return {ra.GetBestSolution(ISCERR),alpha};
}

auto InitTSP(int n, int m, const std::vector<std::vector<db>> &alpha,const std::vector<std::vector<int>>&solution) -> 
        std::vector<std::vector<int>> {
    std::vector<std::vector<int>>ans;
    for(int _=0;_<n;_++){
        std::vector<std::vector<db>> dist(m,std::vector<db>(m,0));
        for(int i=0;i<m;i++){
            for(int j=0;j<m;j++){
                dist[i][j]=alpha[i][j]*solution[i][_]*solution[j][_];
            }
        }
        auto tsp=TSP(m,dist);
        #ifdef ISCERR
        for(auto x:tsp){
            std::cerr<<x<<' ';
        }
        std::cerr<<'\n';
        #endif
        ans.push_back(tsp);
    }
    return ans;
}