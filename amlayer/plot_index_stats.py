#!/usr/bin/env python3
import sys, csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

if len(sys.argv) < 3:
    print("Usage: plot_index_stats.py <index_stats.csv> <out_prefix>")
    sys.exit(1)

csv_path = sys.argv[1]
out_prefix = sys.argv[2]

data = []
with open(csv_path, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data.append(row)

# Helper to filter rows
rows = lambda mode, op: [r for r in data if r['mode']==mode and r['op']==op]

def to_int(r, k):
    try:
        return int(float(r[k]))
    except Exception:
        return 0

def plot_build_compare():
    modes = ['build_incremental','build_sorted']
    metrics = ['lr','lw','pr','pw','hit','miss']
    values = {m: {met:0 for met in metrics} for m in modes}
    for m in modes:
        r = rows(m, 'build')
        if r:
            r = r[-1]  # last occurrence
            for met in metrics:
                values[m][met] = to_int(r, met)
    x = range(len(metrics))
    width = 0.35
    plt.figure(figsize=(10,6))
    plt.bar([i - width/2 for i in x], [values[modes[0]][met] for met in metrics], width, label=modes[0])
    plt.bar([i + width/2 for i in x], [values[modes[1]][met] for met in metrics], width, label=modes[1])
    plt.xticks(list(x), metrics)
    plt.ylabel('Count')
    plt.title('Index build statistics comparison')
    plt.grid(True, linestyle='--', alpha=0.4, axis='y')
    plt.legend()
    plt.tight_layout()
    out = f"{out_prefix}_build_compare.png"
    plt.savefig(out)
    print(f"Saved {out}")
    plt.close()


def plot_scan_compare(op):
    modes = ['build_incremental','build_sorted']
    metrics = ['lr','hit','miss','ms']
    vals = {m: {met:0.0 for met in metrics} for m in modes}
    for m in modes:
        r = rows(m, op)
        if r:
            r = r[-1]
            for met in metrics:
                vals[m][met] = float(r[met])
    x = range(len(metrics))
    width = 0.35
    plt.figure(figsize=(10,6))
    plt.bar([i - width/2 for i in x], [vals[modes[0]][met] for met in metrics], width, label=modes[0])
    plt.bar([i + width/2 for i in x], [vals[modes[1]][met] for met in metrics], width, label=modes[1])
    plt.xticks(list(x), metrics)
    plt.ylabel('Value')
    plt.title(f'Scan ({op}) statistics comparison')
    plt.grid(True, linestyle='--', alpha=0.4, axis='y')
    plt.legend()
    plt.tight_layout()
    out = f"{out_prefix}_{op}_compare.png"
    plt.savefig(out)
    print(f"Saved {out}")
    plt.close()

plot_build_compare()
plot_scan_compare('scan_all')
plot_scan_compare('point_eq')
