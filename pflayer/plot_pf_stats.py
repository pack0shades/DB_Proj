#!/usr/bin/env python3
import csv
import sys
import matplotlib
import matplotlib.pyplot as plt
matplotlib.use('Agg')


if len(sys.argv) < 3:
    print("Usage: plot_pf_stats.py <combined_csv> <output_png>")
    sys.exit(1)

csv_path = sys.argv[1]
out_png = sys.argv[2]

write_pct = []
metrics = {
    'logical_reads': [],
    'logical_writes': [],
    'physical_reads': [],
    'physical_writes': [],
    'buffer_hits': [],
    'buffer_misses': [],
}

with open(csv_path, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        write_pct.append(int(row['write_pct']))
        for k in metrics:
            metrics[k].append(int(row[k]))

plt.figure(figsize=(10, 6))
for name, series in metrics.items():
    plt.plot(write_pct, series, marker='o', label=name)

plt.xlabel('Write percentage')
plt.ylabel('Count')
plt.title('PF statistics vs write/read mixture (LRU)')
plt.grid(True, linestyle='--', alpha=0.5)
plt.legend()
plt.tight_layout()
plt.savefig(out_png)
print(f"Saved plot to {out_png}")
