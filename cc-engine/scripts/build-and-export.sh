#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
engine_dir="$(cd "${script_dir}/.." && pwd)"

cd "${engine_dir}"
bazel run -c opt //:wasm_exporter -- ../canvas-worker/src/wasm
