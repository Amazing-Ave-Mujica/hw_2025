#pragma once
#include <random>
namespace config{
    static constexpr int RANDOM_SEED=0;//随机数种子
    static constexpr int MAX_M=16;//资源种类数
    static constexpr int MAX_N=10;//容器数量
    static constexpr int TIME_SLICE_DIVISOR = 1800;
    static constexpr int T=20000;//模拟退火循环次数
    static constexpr double COOLING_RATE=0.999;//模拟退火降温速率
    static constexpr int MAX_ITER=20000;//模拟退火最大迭代次数
    static constexpr double BETA_VALUE=1;//模拟退火超参数
    static constexpr int SEGMENT_DEFAULT_CAPACITY=10;//段默认容量
    static constexpr int PRINTER_BUF_CAPACITY=(1<<20);//打印缓冲区容量
}  // namespace config