import matplotlib.pyplot as plt
import re
import math

def extract_and_plot_subplot(filename):
    channel_chip_die_plane_data = []
    erase_counts_data = []

    with open(filename, 'r') as file:
        lines = file.readlines()

        for i in range(0, len(lines), 3001):  
            header_line = lines[i].strip()
            match = re.match(r"Channel: (\d+), Chip: (\d+), Die: (\d+), Plane: (\d+)", header_line)
            if match:
                channel, chip, die, plane = map(int, match.groups())
                channel_chip_die_plane_data.append((channel, chip, die, plane))

                erase_counts = []
                for j in range(i + 1, i + 3001):  
                    data_line = lines[j].strip()
                    match = re.match(r"Erase Count (\d+): (\d+) Blocks", data_line)
                    if match:
                        erase_count, block_count = map(int, match.groups())
                        erase_counts.append((erase_count, block_count))
                erase_counts_data.append(erase_counts)

    num_graphs = len(channel_chip_die_plane_data)
    num_cols = 12  
    num_rows = int(math.ceil(num_graphs / num_cols))+1

    fig, axes = plt.subplots(num_rows, num_cols, figsize=(15, num_rows * 5))
    axes = axes.flatten()  

    for idx, (header_info, erase_counts) in enumerate(zip(channel_chip_die_plane_data, erase_counts_data)):
        channel, chip, die, plane = header_info

        x_values = [item[0] for item in erase_counts]
        y_values = [item[1] for item in erase_counts]

        axes[idx].bar(x_values, y_values, color='skyblue', edgecolor='black')
        axes[idx].set_title("Channel: %d, Chip: %d, Die: %d, Plane: %d" % (channel, chip, die, plane))
        axes[idx].set_xlabel("Erase Count")
        axes[idx].set_ylabel("Number of Blocks")
        axes[idx].grid(axis='y', linestyle='--', alpha=0.7)


    for idx in range(num_graphs, len(axes)):
        axes[idx].axis('off')

    plt.tight_layout()
    plt.savefig("combined_erase_histogram.png")
    plt.show()

filename = "erase_histogram.csv"
extract_and_plot_subplot(filename)

