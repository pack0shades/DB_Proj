#!/usr/bin/env python3
"""Combine per-policy PF stats into a single CSV.

Looks for files in this directory matching:
    pf_stats_pool<poolsize>_<policy>_<write_pct>.csv
Each such file is assumed to be a CSV with at least the following columns:
    logical_reads,logical_writes,physical_reads,physical_writes,buffer_hits,buffer_misses
The output combined file will have columns:
    policy,write_pct,logical_reads,logical_writes,physical_reads,physical_writes,buffer_hits,buffer_misses
and be written to 'pf_combined.csv'.
"""
from __future__ import annotations
import csv
import re
from pathlib import Path
from typing import List, Dict

# Now expect write_pct in the filename
INPUT_PATTERN = re.compile(r"^pf_stats_pool(\d+)_([A-Za-z0-9_-]+)_([0-9]+(?:\.[0-9]+)?)\.csv$")

DATA_COLUMNS = [
    "logical_reads",
    "logical_writes",
    "physical_reads",
    "physical_writes",
    "buffer_hits",
    "buffer_misses",
]
OUTPUT_COLUMNS = ["policy", "write_pct"] + DATA_COLUMNS
OUTPUT_FILE = "pf_combined.csv"


def find_input_files(directory: Path) -> List[Path]:
    return [p for p in directory.iterdir() if p.is_file() and INPUT_PATTERN.match(p.name)]


def read_stats_file(path: Path, policy: str, write_pct: str) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        # Ensure all required data columns are present
        missing = [c for c in DATA_COLUMNS if c not in (reader.fieldnames or [])]
        if missing:
            print(f"[WARN] Skipping {path.name}: missing columns {missing}")
            return rows
        for r in reader:
            out_row = {"policy": policy, "write_pct": write_pct}
            for c in DATA_COLUMNS:
                out_row[c] = r.get(c, "")
            rows.append(out_row)
    return rows


def main() -> None:
    here = Path(__file__).resolve().parent
    inputs = find_input_files(here)
    if not inputs:
        print("[INFO] No matching pf_stats_pool*_<policy>_<write_pct>.csv files found.")
        return

    combined: List[Dict[str, str]] = []
    for path in inputs:
        m = INPUT_PATTERN.match(path.name)
        assert m is not None
        pool_size, policy, write_pct = m.groups()
        file_rows = read_stats_file(path, policy, write_pct)
        combined.extend(file_rows)

    if not combined:
        print("[INFO] No rows collected; nothing to write.")
        return

    # Sort for stable output: by policy then numeric write_pct
    def sort_key(r: Dict[str, str]):
        try:
            wp = float(r.get("write_pct", "nan"))
        except ValueError:
            wp = float("inf")
        return (r["policy"], wp)

    combined.sort(key=sort_key)

    out_path = here / OUTPUT_FILE
    with out_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=OUTPUT_COLUMNS)
        writer.writeheader()
        writer.writerows(combined)
    print(f"[INFO] Wrote {len(combined)} rows to {out_path}")


if __name__ == "__main__":
    main()
