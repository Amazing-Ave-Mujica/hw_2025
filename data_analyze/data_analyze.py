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
        # 读取第一行的 T, M, N, V, G
        T, M, N, V, G = map(int, file.readline().split())
        
        # 计算每行的列数
        cols_per_row = math.ceil(T / 1800)
        
        # 读取接下来的 3*M 行数据
        data = []
        for _ in range(3 * M):
            row = list(map(float, file.readline().split()))
            data.append(row)
        
        # 初始化变量
        max_sum = 0
        current_sum = 0
        obj_size_map = {}  # 用于存储 obj_id 和对应的 obj_size
        obj_tag_map={}  # 用于存储 obj_id 和对应的 obj_label
        current_sum=[0 for _ in range(M+1)]
        max_sum=[0 for _ in range(M+1)]
        obj_read_map = {i: [] for i in range(1, M + 1)}  # 用于记录每个 obj_tag 的读取活动

        # 解析每个时间片的数据
        for time_slice in range(T + 105):  # T+105 个时间片
            timestamp=file.readline()
            # 读取删除操作
            n_delete = int(file.readline().strip())
            for _ in range(n_delete):
                obj_id = int(file.readline().strip())  # 删除对象编号
                if obj_id in obj_size_map:
                    current_sum[obj_tag_map[obj_id]] -= obj_size_map[obj_id]  # 减去对应的 obj_size
                    del obj_size_map[obj_id]  # 从字典中移除该对象

            # 读取写入操作
            n_write = int(file.readline().strip())
            for _ in range(n_write):
                obj_id, obj_size, obj_tag = map(int, file.readline().split())
                obj_size_map[obj_id] = obj_size  # 存储 obj_id 和对应的 obj_size
                obj_tag_map[obj_id] = obj_tag  # 存储 obj_id 和对应的 obj_label
                current_sum[obj_tag] += obj_size  # 写入操作增加总大小

            # 读取读取操作
            n_read = int(file.readline().strip())
            for _ in range(n_read):
                req_id, obj_id = map(int, file.readline().split())
                if obj_id in obj_tag_map:
                    obj_tag = obj_tag_map[obj_id]
                    obj_read_map[obj_tag].append((time_slice, obj_id))  # 记录读取活动
            max_sum = [max(a, b) for a, b in zip(max_sum, current_sum)]
    
    return T, M, N, V, G, data, cols_per_row,sum(max_sum),obj_read_map

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

def plot_bar_chart(nv_value, max_sum, save_dir):
    """
    绘制柱状图，显示 N*V 和 max_sum 的对比。
    """
    # 设置柱状图的标签和对应的值
    labels = ['total capacity(N*V)', 'Max Need']
    values = [nv_value, 3*max_sum]

    # 绘制柱状图
    plt.figure(figsize=(8, 6))
    plt.bar(labels, values, color=['blue', 'orange'])
    plt.ylabel('Value')
    plt.title('Comparison')
    plt.grid(axis='y', linestyle='--', alpha=0.7)

    # 保存柱状图
    bar_chart_filename = os.path.join(save_dir, 'nv_vs_max_sum.png')
    plt.savefig(bar_chart_filename)
    plt.close()
    print(f"Bar chart saved as {bar_chart_filename}")

def plot_read_activity(obj_read_map, T, save_dir, M):
    """
    绘制每个 obj_tag 的读取活动图表。
    """
    for obj_tag in range(1, M + 1):
        # 提取当前 obj_tag 的读取活动
        read_times = obj_read_map.get(obj_tag, [])
        x = [time for time, _ in read_times]  # 时间片
        y = [obj_id for _, obj_id in read_times]  # 读取的 obj_id

        # 绘制图表
        plt.figure(figsize=(10, 6))
        plt.scatter(x, y, color='purple', alpha=0.7,s=1, label=f'Read Activity for obj_tag_{obj_tag}')
        plt.xlabel('Time Slice')
        plt.ylabel('Read obj_id')
        plt.title(f'Read Activity for obj_tag_{obj_tag}')
        plt.grid(True)
        plt.legend()

        # 保存图表
        filename = os.path.join(save_dir, f'read_activity_obj_tag_{obj_tag}.png')
        plt.savefig(filename)
        plt.close()
        print(f"Read activity chart saved as {filename}")

def plot_graphs(T, M, N, V, G, data, cols_per_row,max_sum,obj_read_map, save_dir):
    """
    主绘图函数，调用其他绘图功能。
    """
    # 绘制 read 图表
    plot_read_graphs(data, cols_per_row, save_dir, M)

    # 绘制 change_of_label 图表
    plot_change_of_label_graphs(data, cols_per_row, save_dir, M)
    plot_bar_chart(N*V, max_sum, save_dir)
    plot_read_activity(obj_read_map, T, save_dir, M)

def main():
    """
    主函数，解析命令行参数并调用绘图函数。
    """
    # 设置命令行参数
    parser = argparse.ArgumentParser(description="Plot graphs from data file.")
    parser.add_argument('--data_path', type=str, default="../data/sample_offical.in",
                        help='Path to the input data file.')
    parser.add_argument('--save_dir', type=str, default='./output',
                        help='Directory to save the output graphs.')
    args = parser.parse_args()

    # 确保保存目录存在
    os.makedirs(args.save_dir, exist_ok=True)

    # 读取数据并绘制图表
    T, M, N, V, G, data, cols_per_row,max_sum,obj_read_map = read_data(args.data_path)
    plot_graphs(T, M, N, V, G, data, cols_per_row,max_sum,obj_read_map, args.save_dir)
    print(f"Graphs saved successfully in {args.save_dir}!")

if __name__ == "__main__":
    main()

