import numpy as np
import matplotlib.pyplot as plt
import math
import argparse
import os

def read_data(filename):
    """
    读取数据文件并返回解析后的内容。
    """
    with open(filename, 'r') as file:
        # 读取第一行的 T, M, N, V, G,K
        T, M, N, V, G,K = map(int, file.readline().split())
        
        # 计算每行的列数
        cols_per_row = math.ceil(T / 1800)
        
        # 读取接下来的 3*M 行数据
        data = []
        for _ in range(3 * M):
            row = list(map(float, file.readline().split()))
            data.append(row)
        
        # 初始化变量
        max_sum = 0
        max_sum=[0 for _ in range(M+1)]
        for i in range(M):
            cur=0
            for j in range(cols_per_row):
                cur-=data[i][j]
                cur+=data[i+M][j]
                max_sum[i]=max(max_sum[i],cur)
            
        cur=0
        cur_max=0
        for i in range(cols_per_row):
            for j in range(M):
                cur-=data[j][i]
                cur+=data[j+M][i]
            cur_max=max(cur_max,cur)
    
    return T, M, N, V, G, data, cols_per_row,sum(max_sum),cur_max

def plot_line_chart(x, y, title, xlabel, ylabel, filename, labels=None, colors=None):
    """
    绘制折线图并保存到文件。
    """
    plt.figure(figsize=(8, 4))
    if isinstance(y[0], (list, np.ndarray)):  # 多条曲线
        for i, y_data in enumerate(y):
            label = labels[i] if labels else None
            color = colors[i] if colors else None
            plt.plot(x, y_data, marker='o', linestyle='-', label=label, color=color)
    else:  # 单条曲线
        plt.plot(x, y, marker='o', linestyle='-')

    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    if labels:
        plt.legend()
    plt.grid(True)
    plt.savefig(filename)
    plt.close()

def plot_read_graphs(data, cols_per_row, save_dir, M):
    """
    绘制 read 图表。
    """
    for i in range(2 * M, 3 * M):
        title = f'read_{i + 1 - 2 * M}'
        filename = os.path.join(save_dir, title + '.png')
        x = np.arange(cols_per_row)
        y = np.array(data[i])
        plot_line_chart(x, y, title, 'Time', 'Number of Operations', filename)

def plot_change_of_label_graphs(data, cols_per_row, save_dir, M):
    """
    绘制 change_of_label 图表。
    """
    for i in range(M):
        diff = np.array(data[i + M]) - np.array(data[i])
        prefix_sum = np.cumsum(diff)
        title = f'change_of_label_{i + 1}'
        filename = os.path.join(save_dir, title + '.png')
        x = np.arange(cols_per_row)
        y = [prefix_sum, data[i], data[i + M]]
        labels = ['object_left', 'object_deleted', 'object_added']
        colors = ['orange', 'blue', 'green']
        plot_line_chart(x, y, title, 'Time', 'object count', filename, labels, colors)

def plot_bar_chart(nv_value, max_sum,max_cur, save_dir):
    """
    绘制柱状图，显示 N*V 和 max_sum 的对比。
    """
    # 设置柱状图的标签和对应的值
    labels = ['total capacity(N*V)', 'Max Need','Max Cur']
    values = [nv_value, 3*max_sum,3*max_cur]

    # 绘制柱状图
    plt.figure(figsize=(8, 6))
    plt.bar(labels, values, color=['blue', 'orange','red'])
    plt.ylabel('Value')
    plt.title('Comparison')
    plt.grid(axis='y', linestyle='--', alpha=0.7)

    # 保存柱状图
    bar_chart_filename = os.path.join(save_dir, 'nv_vs_max_sum.png')
    plt.savefig(bar_chart_filename)
    plt.close()
    print(f"Bar chart saved as {bar_chart_filename}")

def plot_graphs(T, M, N, V, G, data, cols_per_row,max_sum,max_cur,save_dir):
    """
    主绘图函数，调用其他绘图功能。
    """
    # 绘制 read 图表
    plot_read_graphs(data, cols_per_row, save_dir, M)

    # 绘制 change_of_label 图表
    plot_change_of_label_graphs(data, cols_per_row, save_dir, M)
    plot_bar_chart(N*V, max_sum,max_cur, save_dir)

def main():
    """
    主函数，解析命令行参数并调用绘图函数。
    """
    # 设置命令行参数
    parser = argparse.ArgumentParser(description="Plot graphs from data file.")
    parser.add_argument('--data_path', type=str, default="./data/sample_practice.in",
                        help='Path to the input data file.')
    parser.add_argument('--save_dir', type=str, default='./data_analyze/output',
                        help='Directory to save the output graphs.')
    args = parser.parse_args()

    # 确保保存目录存在
    os.makedirs(args.save_dir, exist_ok=True)

    # 读取数据并绘制图表
    T, M, N, V, G, data, cols_per_row,max_sum,max_cur = read_data(args.data_path)
    plot_graphs(T, M, N, V, G, data, cols_per_row,max_sum, max_cur,args.save_dir)
    print(f"Graphs saved successfully in {args.save_dir}!")

if __name__ == "__main__":
    main()

