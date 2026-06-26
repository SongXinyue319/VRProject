#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="$ROOT_DIR/result/experiments_$STAMP"
SUMMARY="$OUT_DIR/summary.csv"

mkdir -p "$OUT_DIR"

command -v cmake >/dev/null 2>&1 || {
  echo "cmake is required. Install it with: sudo apt update && sudo apt install -y cmake build-essential"
  exit 1
}

echo "Configuring Release build..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "Building RayTracing..."
cmake --build "$BUILD_DIR" --config Release -j"$(nproc)"

# 固定线程数，保证 BVH 与无 BVH 两种模式在相同硬件/并行度下公平对比、可复现
export OMP_NUM_THREADS="${OMP_NUM_THREADS:-32}"
echo "OMP_NUM_THREADS=$OMP_NUM_THREADS (cores=$(nproc))"
{
  echo "date=$(date -Iseconds)"
  echo "host=$(hostname)"
  echo "cores=$(nproc)"
  echo "OMP_NUM_THREADS=$OMP_NUM_THREADS"
  echo "spp=1 (timing 用单采样，与原方法学一致；beauty 图另行 spp=2 渲染)"
  echo "build=Release -O3 + OpenMP + SAH-BVH"
} > "$OUT_DIR/meta.txt"

EXE="$BUILD_DIR/RayTracing"
if [[ ! -x "$EXE" && -x "$BUILD_DIR/Release/RayTracing" ]]; then
  EXE="$BUILD_DIR/Release/RayTracing"
fi

if [[ ! -x "$EXE" ]]; then
  echo "Cannot find RayTracing executable under $BUILD_DIR"
  exit 1
fi

cat > "$SUMMARY" <<CSV
model,vertices,faces,bvh_seconds,no_bvh_seconds,speedup
CSV

run_case() {
  local name="$1"
  local model="$2"
  local vertices="$3"
  local faces="$4"

  local bvh_log="$OUT_DIR/${name}_bvh.log"
  local no_bvh_log="$OUT_DIR/${name}_no_bvh.log"
  local bvh_img="$OUT_DIR/${name}_bvh.ppm"
  local no_bvh_img="$OUT_DIR/${name}_no_bvh.ppm"

  echo
  echo "=== $name: BVH ==="
  (
    cd "$BUILD_DIR"
    "$EXE" "$model"
  ) | tee "$bvh_log"
  if [[ -f "$BUILD_DIR/binary.ppm" ]]; then
    mv "$BUILD_DIR/binary.ppm" "$bvh_img"
  fi

  echo
  echo "=== $name: no BVH/check ==="
  (
    cd "$BUILD_DIR"
    "$EXE" "$model" check
  ) | tee "$no_bvh_log"
  if [[ -f "$BUILD_DIR/binary.ppm" ]]; then
    mv "$BUILD_DIR/binary.ppm" "$no_bvh_img"
  fi

  local bvh_seconds
  local no_bvh_seconds
  bvh_seconds="$(awk '/Render seconds:/ { value=$3 } END { print value }' "$bvh_log")"
  no_bvh_seconds="$(awk '/Render seconds:/ { value=$3 } END { print value }' "$no_bvh_log")"

  local speedup
  speedup="$(awk -v n="$no_bvh_seconds" -v b="$bvh_seconds" 'BEGIN { if (b > 0) printf "%.4f", n / b; else print "NA" }')"

  echo "$name,$vertices,$faces,$bvh_seconds,$no_bvh_seconds,$speedup" >> "$SUMMARY"
}

run_case "bunny" "../models/bunny/bunny.obj" "2503" "4968"
run_case "dragon_res3" "../models/stanford_dragon/dragon_vrip_res3.obj" "22998" "47794"
run_case "dragon_res2" "../models/stanford_dragon/dragon_vrip_res2.obj" "100250" "202520"
run_case "armadillo" "../models/stanford_armadillo/armadillo.obj" "172974" "345944"

echo
echo "Experiment complete."
echo "Summary: $SUMMARY"
echo
column -s, -t "$SUMMARY" 2>/dev/null || cat "$SUMMARY"
