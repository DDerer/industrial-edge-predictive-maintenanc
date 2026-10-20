#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 "$project_dir/tools/stm32_simulator.py" --frames 12 --delay 0.01 \
    | "$project_dir/build/gateway/edge_collector" --input - --max-frames 12

