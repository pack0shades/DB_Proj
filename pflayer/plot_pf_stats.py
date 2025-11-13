#!/usr/bin/env python3
import csv
import sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

if len(sys.argv) < 3:
    print("Usage: plot_pf_stats.py <combined_csv> <output_prefix>")
    sys.exit(1)

csv_path = sys.argv[1]
out_prefix = sys.argv[2]

# Prepare containers per policy
policies = {}
metric_names = ['logical_reads','logical_writes','physical_reads','physical_writes','buffer_hits','buffer_misses']

with open(csv_path, 'r') as f:
    reader = csv.DictReader(f)
    has_policy = 'policy' in reader.fieldnames
    for row in reader:
        policy = row['policy'] if has_policy else 'LRU'
        d = policies.setdefault(policy, {'write_pct': [], **{k: [] for k in metric_names}})
        d['write_pct'].append(int(row['write_pct']))
        for k in metric_names:
            d[k].append(int(row[k]))

# Plot one figure per policy
for policy, data in policies.items():
    # sort by write_pct
    order = sorted(range(len(data['write_pct'])), key=lambda i: data['write_pct'][i])
    x = [data['write_pct'][i] for i in order]

    plt.figure(figsize=(10, 6))
    for name in metric_names:
        y = [data[name][i] for i in order]
        plt.plot(x, y, marker='o', label=name)
    plt.xlabel('Write percentage')
    plt.ylabel('Count')
    plt.title(f'PF statistics vs write/read mixture ({policy})')
    plt.grid(True, linestyle='--', alpha=0.5)
    plt.legend()
    plt.tight_layout()
    out_png = f"{out_prefix}_{policy}.png"
    plt.savefig(out_png)
    print(f"Saved plot to {out_png}")
    plt.close()
