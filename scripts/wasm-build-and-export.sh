#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
engine_dir="${repo_root}/cc-engine"
worker_dir="${repo_root}/canvas-worker"

cd "${engine_dir}"

# Canonical interactive/agent Wasm build. Keep the exporter in the same
# optimized configuration so both commands share Bazel action-cache entries.
bazel build -c opt //:demo_wasm
bazel run -c opt //:wasm_exporter -- "${worker_dir}/src/wasm"

# The app imports the package's dist entrypoint, so keep its generated worker
# and Wasm payload synchronized with the freshly exported source artifacts.
cd "${worker_dir}"
bun run build
