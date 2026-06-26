#!/usr/bin/env bash
# 用优化后的渲染器重渲染各模型 beauty 图（BVH 模式、2x2 抗锯齿、含真高光），
# 输出到 images/ 与 report/figures/。需先 build 好 RayTracing。
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
EXE="$BUILD/RayTracing"
export OMP_NUM_THREADS="${OMP_NUM_THREADS:-32}"
SPP="${SPP:-2}"

render() {
  local name="$1" model="$2"
  echo "=== render $name (BVH, spp=$SPP) ==="
  ( cd "$BUILD" && "$EXE" "$model" "spp=$SPP" ) | grep -E "Render seconds" || true
  python3 - "$BUILD/binary.ppm" "$ROOT/images/${name}_bvh.png" "$ROOT/report/figures/${name}_bvh.png" <<'PY'
import sys
from PIL import Image
src, *dsts = sys.argv[1:]
im = Image.open(src)
for d in dsts:
    im.save(d)
print("  saved", *dsts)
PY
}

render "bunny"       "../models/bunny/bunny.obj"
render "dragon_res3" "../models/stanford_dragon/dragon_vrip_res3.obj"
render "dragon_res2" "../models/stanford_dragon/dragon_vrip_res2.obj"
render "armadillo"   "../models/stanford_armadillo/armadillo.obj"

# bunny check 图（验证逐像素一致用，spp=1 与 BVH spp=1 对比；这里出 spp=2 展示一致）
( cd "$BUILD" && "$EXE" "../models/bunny/bunny.obj" check "spp=$SPP" ) >/dev/null
python3 -c "from PIL import Image; Image.open('$BUILD/binary.ppm').save('$ROOT/images/bunny_check.png'); Image.open('$BUILD/binary.ppm').save('$ROOT/report/figures/bunny_check.png')"
echo "beauty 渲染完成"
