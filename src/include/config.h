#pragma once
#pragma optimize(2)
using db = double;
namespace config {

// #define LLDB
// #define SINGLE_READ_MODE
#define ISCERR
#define USINGTSP      // 是否使用TSP
// #define WRITE_BALANCE // 是否按照磁盘剩余空间排序

static constexpr int RANDOM_SEED = 0; // 随机数种子
static constexpr int MAX_M = 16;      // 资源种类数
static constexpr int MAX_N = 10;      // 容器数量
static constexpr int TIME_SLICE_DIVISOR = 1800;

static constexpr int T = 200000;           // 模拟退火初始温度
static constexpr db EPS_T = 1e-8;         // 模拟退火终止温度
static constexpr db COOLING_RATE = 0.9999; // 模拟退火降温速率
static constexpr int MAX_ITER = 2000000;    // 模拟退火最大迭代次数
static constexpr db BETA_VALUE = 1;       // 模拟退火、遗传算法超参数
static constexpr db GAMA_VALUE = 1;       // 遗传算法超参数，正则项系数

static constexpr int SEGMENT_DEFAULT_CAPACITY = 10;    // 段默认容量
static constexpr int PRINTER_BUF_CAPACITY = (1 << 20); // 打印缓冲区容量

static constexpr int K_POP_SIZE = 300;    // 种群大小
static constexpr int K_MAX_GEN = 3000;    // 最大迭代次数
static constexpr db K_CROSS_RATE = 0.5;   // 交叉概率
static constexpr db K_MUTATE_RATE = 0.05; // 变异概率
static constexpr int ELITE_NUM = 10;      // 精英个体数量

static constexpr int DISK_READ_FETCH_LEN = 63; // 规划最近的读取任务个数
int RTQ_DISK_PART_SIZE;                        // RTQ 给磁盘分区大小 NOLINT
int JUMP_THRESHOLD; // 热门块比当前块大多少时直接跳转 NOLINT

static constexpr db INF = 1e18; // 无穷大

constexpr bool USE_COMPACT = true;

// NOLINTNEXTLINE
enum WRITEPOLICIES {
  none = 0, // 默认策略
  compact   // 只在硬盘的前1/3读数据
};

constexpr auto WritePolicy() {
  if (USE_COMPACT) {
    return WRITEPOLICIES::compact;
  }
  return WRITEPOLICIES::none;
};

} // namespace config