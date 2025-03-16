#include <bits/stdc++.h>
// encode in UTF-8
// https://cloud.tencent.com/developer/article/2149801
// none 策略: 不优化
constexpr int EXTRA_TIME = 105;
int Timeslice, Mtag, Ndisk, Vdisk_size, Gdisk_token;
struct Object
{
};

struct IO_none
{
    void delete_op()
    {
        int n_delete;
        std::cin >> n_delete;
    }
    void write_op()
    {
    }
    void read_op()
    {
    }
};


int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cin >> Timeslice >> Mtag >> Ndisk >> Vdisk_size >> Gdisk_token;
    // 预处理跳过
    for (int i = 0; i < Mtag; i++)
    {
        for (int j = 0, m = (Timeslice - 1) / 1800 + 1; j < m; j++)
        {
            int x;
            std::cin >> x;
        }
    }
    for (int i = 0; i < Mtag; i++)
    {
        for (int j = 0, m = (Timeslice - 1) / 1800 + 1; j < m; j++)
        {
            int x;
            std::cin >> x;
        }
    }
    for (int i = 0; i < Mtag; i++)
    {
        for (int j = 0, m = (Timeslice - 1) / 1800 + 1; j < m; j++)
        {
            int x;
            std::cin >> x;
        }
    }
    std::cout << "OK";
    std::fflush(stdout);
    IO_none WOW;
    for (int T = 0; T < Timeslice + EXTRA_TIME; T++)
    {
        WOW.delete_op();
        WOW.write_op();
        WOW.read_op();
    }
}