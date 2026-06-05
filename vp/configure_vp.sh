#!/usr/bin/env bash
# Export VP build env and run cmake. Install paths override bashrc (not :=).
# Override VP_SYSC_BACKEND / CMAKE_* via VAR=value; custom SystemC: edit paths below or cmake -D.
#   ./configure_vp.sh
#   CMAKE_CXX_STANDARD=17 ./configure_vp.sh
#   source ./configure_vp.sh

set -euo pipefail

VP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${VP_DIR}/.." && pwd)"

# ---------------------------------------------------------------------------
# Configuration — edit defaults here
# ---------------------------------------------------------------------------
: "${CMAKE_BUILD_TYPE:=Debug}"             # Debug | Release
: "${CMAKE_CXX_STANDARD:=17}"              # 17 | 20 

# Install paths always from install_c17/c20 (:= would keep stale SYSTEMC_HOME from bashrc).
if [[ "${CMAKE_CXX_STANDARD}" == 17 ]]; then
  SYSTEMC_HOME="/usr/local/systemc301"
  CCI_HOME="/usr/local/cci"
  OPENSSL_ROOT="/usr"
  BOOST_ROOT="/usr"
else
  SYSTEMC_HOME="/usr/local/systemc301"
  CCI_HOME="/usr/local/cci"
  OPENSSL_ROOT="/usr"
  BOOST_ROOT="/usr"
fi

CMAKE_EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) sed -n '2,5p' "$0" | sed 's/^# \?//'; exit 0 ;;
    --) shift; CMAKE_EXTRA=("$@"); break ;;
    *) echo "Unknown: $1 (use VAR=value or -- cmake-args)" >&2; exit 1 ;;
  esac
done

[[ "${CMAKE_CXX_STANDARD}" =~ ^(17|20)$ ]] || { echo "CMAKE_CXX_STANDARD must be 17 or 20" >&2; exit 1; }

export OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-${OPENSSL_ROOT}}"
export SYSTEMC_HOME CCI_HOME OPENSSL_ROOT BOOST_ROOT
export BOOST_INC="${BOOST_ROOT}/include" BOOST_LIB="${BOOST_ROOT}/lib"
export OPENSSL_INC="${OPENSSL_ROOT}/include" OPENSSL_LIB="${OPENSSL_ROOT}/lib64"
export CRYPTO_LIB="${OPENSSL_LIB}"
export CMAKE_BUILD_TYPE CMAKE_CXX_STANDARD CXX_STD="c++${CMAKE_CXX_STANDARD}"

libdir() { [[ -d "$1/lib-linux64" ]] && echo "$1/lib-linux64" || echo "$1/lib64"; }
_sc_lib="$(libdir "${SYSTEMC_HOME}")"
_cci_lib="$(libdir "${CCI_HOME}")"
export CMAKE_PREFIX_PATH="${SYSTEMC_HOME};${CCI_HOME};${BOOST_ROOT};${OPENSSL_ROOT_DIR}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}"
export LD_LIBRARY_PATH="${OPENSSL_LIB}:${BOOST_LIB}:${_sc_lib}:${_cci_lib}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export PATH="${OPENSSL_ROOT}/bin:${BOOST_ROOT}/bin:${PATH}"

echo " CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
echo "SYSTEMC_HOME=${SYSTEMC_HOME}"

[[ "${BASH_SOURCE[0]}" != "${0}" ]] && return 0

BUILD_DIR="${VP_DIR}/build"

CMAKE_ARGS=(
  -S "${VP_DIR}" -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  -DCMAKE_CXX_STANDARD="${CMAKE_CXX_STANDARD}"
  -DCMAKE_CXX_STANDARD_REQUIRED=ON
  -DCMAKE_CXX_EXTENSIONS=OFF
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  -DBOOST_ROOT="${BOOST_ROOT}" -DBoost_ROOT="${BOOST_ROOT}"
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}"
)
  CMAKE_ARGS+=(
    -DSYSTEMC_HOME="${SYSTEMC_HOME}" -DCCI_HOME="${CCI_HOME}"
    -DBoost_NO_SYSTEM_PATHS=ON -DBoost_NO_BOOST_CMAKE=ON
    -DCMAKE_BUILD_RPATH="${OPENSSL_LIB};${BOOST_LIB};${_sc_lib};${_cci_lib}"
    -DCMAKE_INSTALL_RPATH="${OPENSSL_LIB};${BOOST_LIB};${_sc_lib};${_cci_lib}"
  )

mkdir -p "${BUILD_DIR}"
cmake "${CMAKE_ARGS[@]}" "${CMAKE_EXTRA[@]}"
  echo "Configured ${BUILD_DIR}"
  echo " cd $(basename "${BUILD_DIR}") && make sep-vp"
