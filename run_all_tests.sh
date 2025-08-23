#!/usr/bin/env bash
set -euo pipefail

# --- Configuration ---
BUILD_DIR="build"
LOG_DIR="$BUILD_DIR/logs"
declare -A EXE_MAP=(
  [test_LruOnly]="LRU"
  [test_LfuCache]="LFU"
  [test_ArcCache]="ARC"
  [test_ArcNew]="ARC_new"
)

# --- Build ---
echo "🔧 Configuring & Building..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null

# --- Run and record logs ---
mkdir -p "$LOG_DIR"
echo "🚀 Running tests..."
for exe in "${!EXE_MAP[@]}"; do
  path="$BUILD_DIR/$exe"
  if [[ -x "$path" ]]; then
    echo "  - $exe"
    "$path" | tee "$LOG_DIR/${exe}.log" >/dev/null
  else
    echo "  - (skip) $exe not found"
  fi
done

# --- Parse and compare (use built-in Python) ---
python3 <<'PY'
import re, os, glob, csv, sys, textwrap, math

BUILD_DIR="build"
LOG_DIR=os.path.join(BUILD_DIR,"logs")
os.makedirs(LOG_DIR, exist_ok=True)

exe_to_algo = {
    "test_LruOnly": "LRU",
    "test_LfuCache": "LFU",
    "test_ArcCache": "ARC",
    "test_ArcNew": "ARC_new",
}

header_re = re.compile(r"^===\s*(.+?)\s*===\s*$")
hit_re    = re.compile(r"Hit Rate:\s*([0-9.]+)%")

def normalize_scenario(name: str) -> str:
    # Remove algorithm name + Test number from prefix, unify spaces
    s = name.strip()
    s = re.sub(r"^(LRU|Lru|LFU|Arc[_\s]*new|ARC)[^:]*:\s*", "", s, flags=re.IGNORECASE)  # Remove "ARC Test 1:" etc.
    s = re.sub(r"^Test\s*\d+:\s*", "", s, flags=re.IGNORECASE)
    s = re.sub(r"\s+", " ", s).strip()
    return s or name

# Read each log: capture (scenario -> hit rate)
data = {}  # scenario -> { algo: hitrate }
for log in glob.glob(os.path.join(LOG_DIR, "*.log")):
    exe = os.path.splitext(os.path.basename(log))[0]
    algo = exe_to_algo.get(exe)
    if not algo:
        continue

    with open(log, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    current_name = None
    for line in lines:
        m = header_re.match(line.strip())
        if m:
            current_name = normalize_scenario(m.group(1))
            continue
        m2 = hit_re.search(line)
        if m2 and current_name:
            try:
                rate = float(m2.group(1))
            except ValueError:
                continue
            data.setdefault(current_name, {})[algo] = rate
            current_name = None  # End of a scenario

# Output Markdown table
algos = ["LRU","LFU","ARC","ARC_new"]
scenarios = sorted(data.keys())

def fmt(v):
    return ("{:.2f}%".format(v)) if isinstance(v, (int,float)) else v

print("\n📊 Comparison (higher is better)\n")
header = ["Scenario"] + algos
rows = []
for sc in scenarios:
    row = [sc]
    for a in algos:
        row.append(fmt(data.get(sc,{}).get(a, "-")))
    rows.append(row)

# Calculate column width and print simple table
colw = [max(len(str(header[i])), max((len(str(r[i])) for r in rows), default=0)) for i in range(len(header))]
def pr(line): print(line)
pr(" | ".join(h.ljust(colw[i]) for i,h in enumerate(header)))
pr("-|-".join("-"*colw[i] for i in range(len(header))))
for r in rows:
    pr(" | ".join(str(r[i]).ljust(colw[i]) for i in range(len(header))))

# Write CSV
csv_path = os.path.join(LOG_DIR, "summary.csv")
with open(csv_path, "w", newline="", encoding="utf-8") as cf:
    cw = csv.writer(cf)
    cw.writerow(header)
    for r in rows:
        cw.writerow(r)
print(f"\n📁 CSV saved: {csv_path}")

# Simple champion list
print("\n🏆 Winners per scenario:")
for sc in scenarios:
    best_algo = None
    best_val = -1.0
    for a in algos:
        v = data.get(sc,{}).get(a)
        if isinstance(v, (int,float)) and v > best_val:
            best_val = v; best_algo = a
    if best_algo is None:
        line = f"  - {sc}: (no comparable results)"
    else:
        line = f"  - {sc}: {best_algo} @ {best_val:.2f}%"
    print(line)
print()
PY

echo "✅ Done."
