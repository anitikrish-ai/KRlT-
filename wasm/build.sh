#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="/opt/krita-wasm-build"
OUT_DIR="${APP_ROOT}/public/wasm"

source /opt/emsdk/emsdk_env.sh

echo "[1/3] Configuring Krita WebAssembly build with Qt 6.8.3..."
/opt/qt/6.8.3/wasm_singlethread/bin/qt-cmake \
  -S "${SCRIPT_DIR}" \
  -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH=/opt/qt/6.8.3/gcc_64 \
  -DKRITA_SOURCE_DIR=/opt/krita \
  -DWASM_DEPS_PREFIX=/opt/wasm-deps/install

echo "[2/3] Compiling real KDE/krita C++ libraries + Qt6 WASM executable..."
cmake --build "${BUILD_DIR}" -j4

echo "[3/3] Publishing WebAssembly runtime artifacts to ${OUT_DIR}..."
mkdir -p "${OUT_DIR}"
cp "${BUILD_DIR}/krita_wasm.js" "${OUT_DIR}/krita_wasm.js"
cp "${BUILD_DIR}/krita_wasm.wasm" "${OUT_DIR}/krita_wasm.wasm"
cp "${BUILD_DIR}/qtloader.js" "${OUT_DIR}/qtloader.js"
if [ -f "${BUILD_DIR}/krita_wasm.worker.js" ]; then
  cp "${BUILD_DIR}/krita_wasm.worker.js" "${OUT_DIR}/krita_wasm.worker.js"
fi

ls -lh "${OUT_DIR}"
