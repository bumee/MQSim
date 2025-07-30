import numpy as np
import matplotlib.pyplot as plt

# 파일 경로
filename = "workload.IO_Flow.No_0.log"

# 데이터 읽기
latencies = []
with open(filename, "r") as f:
    next(f)  # 첫 줄(헤더) 건너뜀
    for line in f:
        if line.strip() == "":
            continue
        parts = line.strip().split('\t')
        if len(parts) < 2:
            continue
        latencies.append(float(parts[1]))

latencies = np.array(latencies)
median_latency = np.median(latencies)

# x축: 인덱스, y축: latency 값
plt.figure(figsize=(12, 6))
plt.plot(latencies, marker='o', linestyle='-', color='skyblue', label='Latency')
plt.axhline(median_latency, color='red', linestyle='dashed', linewidth=2, label=f'Median: {median_latency:.2f} us')

plt.title('Latency Sequence')
plt.xlabel('Index')
plt.ylabel('ReponseTime (us)')
plt.yscale('log')  # y축 로그 스케일
plt.legend()
plt.grid(True, linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig('latency_sequence.png')