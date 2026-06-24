#!/usr/bin/env bash
# Export VP build env and run cmake. Install paths override bashrc (not :=).
# Override VP_SYSC_BACKEND / CMAKE_* via VAR=value; custom SystemC: edit paths below or cmake -D.
#
# To set the environment with default settings, do the following
#   source ./configure_vp.sh
# To change the defaults, following is an example
#   CMAKE_CXX_STANDARD=20
#   source ./configure_vp.sh

set -euo pipefail

VP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${VP_DIR}/.." && pwd)"

# ---------------------------------------------------------------------------
# Configuration — edit defaults here
# ---------------------------------------------------------------------------
: "${CMAKE_BUILD_TYPE:=Debug}"             # Debug | Release
: "${CMAKE_CXX_STANDARD:=17}"              # 17 | 20

if [[ "${CMAKE_CXX_STANDARD}" == 17 ]]; then
  SYSTEMC_HOME="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c17/systemc-3.0.1"
  CCI_HOME="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c17/cci-1.0.1"
  OPENSSL_ROOT="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c17/openssl-3.0.13"
  BOOST_ROOT="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c17/boost-1.84.0"
else
  SYSTEMC_HOME="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c20/systemc-3.0.1"
  CCI_HOME="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c20/cci-1.0.1"
  OPENSSL_ROOT="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c20/openssl-3.0.13"
  BOOST_ROOT="/localdev/ctr-mharshavardhana/vayavyalabs/installs/install_c20/boost-1.84.0"
fi

CMAKE_EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) sed -n '2,5p' "$0" | sed 's/^# \?//'; exit 0 ;;
    --) shift; CMAKE_EXTRA=("$@"); break ;;
    *) echo "Unknown: $1 (use VAR=value or -- cmake-args)" >&2; exit 1 ;;
  esac
done

[[ "${CMAKE_CXX_STANDARD}" =~ ^(17|20)$ ]] || {
  echo "CMAKE_CXX_STANDARD must be 17 or 20" >&2
  exit 1
  }

export OPENSSL_ROOT_DIR="${OPENSSL_ROOT}"
export SYSTEMC_HOME CCI_HOME OPENSSL_ROOT BOOST_ROOT
export BOOST_INC="${BOOST_ROOT}/include"
export BOOST_LIB="${BOOST_ROOT}/lib"
export OPENSSL_INC="${OPENSSL_ROOT}/include"
export OPENSSL_LIB="${OPENSSL_ROOT}/lib64"
export CRYPTO_LIB="${OPENSSL_LIB}"
export CMAKE_BUILD_TYPE CMAKE_CXX_STANDARD CXX_STD="c++${CMAKE_CXX_STANDARD}"

libdir() { [[ -d "$1/lib-linux64" ]] && echo "$1/lib-linux64" || echo "$1/lib64"; }
_sc_lib="$(libdir "${SYSTEMC_HOME}")"
_cci_lib="$(libdir "${CCI_HOME}")"
export CMAKE_PREFIX_PATH="${SYSTEMC_HOME};${CCI_HOME};${BOOST_ROOT};${OPENSSL_ROOT_DIR}"
export LD_LIBRARY_PATH="${OPENSSL_LIB}:${BOOST_LIB}:${_sc_lib}:${_cci_lib}"
export PATH="${OPENSSL_ROOT}/bin:${BOOST_ROOT}/bin:${PATH}"

echo "SYSTEMC_HOME=${SYSTEMC_HOME}"
echo "CCI_HOME=${CCI_HOME}"
echo "OPENSSL_ROOT=${OPENSSL_ROOT}"
echo "CXX_STD=${CXX_STD}"
echo "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
echo "BOOST_INC=${BOOST_INC}"
echo "BOOST_LIB=${BOOST_LIB}"
echo "OPENSSL_INC=${OPENSSL_INC}"
echo "OPENSSL_LIB=${OPENSSL_LIB}"
echo "CRYPTO_LIB=${CRYPTO_LIB}"

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
