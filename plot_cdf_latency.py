import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read the log file
df = pd.read_csv('workload.IO_Flow.No_0.log', sep='\t')

# Extract EndtoEndDelay column
latencies = df['EndToEndDelay(us)'].values

# Sort latencies for CDF
sorted_latencies = np.sort(latencies)
p = 1. * np.arange(len(sorted_latencies)) / (len(sorted_latencies) - 1)

# Create the plot
plt.figure(figsize=(10, 6))
plt.plot(sorted_latencies, p, 'b-', linewidth=2)
plt.grid(True, linestyle='--', alpha=0.7)

# Set labels and title
plt.xlabel('End-to-End Latency (μs)', fontsize=12)
plt.ylabel('Cumulative Probability', fontsize=12)
plt.title('End-to-End Latency CDF', fontsize=14)

# Add 99th and 99.9th percentile lines
p95 = np.percentile(latencies, 95)
p99 = np.percentile(latencies, 99)
p999 = np.percentile(latencies, 99.9)


plt.axvline(x=p95, color='r', linestyle='--', alpha=0.5, label='95th percentile')
plt.axvline(x=p99, color='r', linestyle='--', alpha=0.5, label='99th percentile')
plt.axvline(x=p999, color='g', linestyle='--', alpha=0.5, label='99.9th percentile')

# Add text annotations for the percentiles
plt.text(p95*1.05, 0.5, f'95th: {p95:.2f}μs', rotation=90)
plt.text(p99*1.05, 0.5, f'99th: {p99:.2f}μs', rotation=90)
plt.text(p999*1.05, 0.5, f'99.9th: {p999:.2f}μs', rotation=90)

plt.legend()

# Use log scale for x-axis to better show the tail
plt.xscale('log')

# Save the plot
plt.savefig('latency_cdf.png', dpi=300, bbox_inches='tight')
print(f"95th percentile latency: {p95:.2f}μs")
print(f"99th percentile latency: {p99:.2f}μs")
print(f"99.9th percentile latency: {p999:.2f}μs")
print(f"Maximum latency: {np.max(latencies):.2f}μs")
print(f"Minimum latency: {np.min(latencies):.2f}μs")
print(f"Mean latency: {np.mean(latencies):.2f}μs")
print(f"Median latency: {np.median(latencies):.2f}μs") 