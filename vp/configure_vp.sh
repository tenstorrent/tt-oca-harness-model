#!/usr/bin/env bash
# Export VP build env and optionally run cmake.
#
#   source ./configure_vp.sh              # export env only
#   ./configure_vp.sh                     # export env + cmake configure
#   CMAKE_CXX_STANDARD=20 source ./configure_vp.sh
#   ./configure_vp.sh -- -DFOO=bar
#
# Edit install paths below. Defaults: CMAKE_BUILD_TYPE=Debug, CMAKE_CXX_STANDARD=20.
# Pre-set env vars override the paths (e.g. SYSTEMC_HOME=/other source ./configure_vp.sh).
# Paths are exported so FindCCI/FindSystemC ($ENV{CCI_HOME}) work when you run cmake manually.

_vp_configure_sourced=false
if [ -n "${BASH_VERSION:-}" ]; then
  [[ "${BASH_SOURCE[0]:-}" != "${0:-}" ]] && _vp_configure_sourced=true
elif [ -n "${ZSH_VERSION:-}" ]; then
  case ${ZSH_EVAL_CONTEXT:-} in
    *:file) _vp_configure_sourced=true ;;
  esac
fi

if ! "${_vp_configure_sourced}"; then
  set -euo pipefail
fi

if [ -n "${BASH_VERSION:-}" ]; then
  _vp_configure_sh="${BASH_SOURCE[0]}"
elif [ -n "${ZSH_VERSION:-}" ]; then
  _vp_configure_sh="${(%):-%N}"
else
  _vp_configure_sh="$0"
fi
VP_DIR="$(cd "$(dirname "${_vp_configure_sh}")" && pwd)"
unset _vp_configure_sh

# ---------------------------------------------------------------------------
# Install paths — edit for your machine
# ---------------------------------------------------------------------------
: "${SYSTEMC_HOME:=/opt/systemc-3.0.1}"
: "${CCI_HOME:=/opt/cci-1.0.2}"
: "${OPENSSL_ROOT:=/usr}"
: "${BOOST_ROOT:=/usr}"

: "${CMAKE_BUILD_TYPE:=Debug}"
: "${CMAKE_CXX_STANDARD:=20}"

CMAKE_EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      sed -n '2,11p' "$0" | sed 's/^# \?//'
      exit 0
      ;;
    --) shift; CMAKE_EXTRA=("$@"); break ;;
    *) echo "Unknown: $1" >&2; exit 1 ;;
  esac
done

# shellcheck source=vp_build_env.sh
source "${VP_DIR}/vp_build_env.sh"
vp_export_build_paths || exit 1

export SYSTEMC_HOME CCI_HOME OPENSSL_ROOT BOOST_ROOT
export CMAKE_BUILD_TYPE CMAKE_CXX_STANDARD

if [[ -z "${VP_CONFIGURE_QUIET:-}" ]]; then
  vp_print_build_env
fi

if "${_vp_configure_sourced}"; then
  unset _vp_configure_sourced
  return 0 2>/dev/null || exit 0
fi
unset _vp_configure_sourced

BUILD_DIR="${VP_DIR}/build"
CMAKE_ARGS=(
  -S "${VP_DIR}" -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  -DCMAKE_CXX_STANDARD="${CMAKE_CXX_STANDARD}"
  -DCMAKE_CXX_STANDARD_REQUIRED=ON
  -DCMAKE_CXX_EXTENSIONS=OFF
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  -DBOOST_ROOT="${BOOST_ROOT}" -DBoost_ROOT="${BOOST_ROOT}"
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT}"
  -DSYSTEMC_HOME="${SYSTEMC_HOME}" -DCCI_HOME="${CCI_HOME}"
  -DSystemCLanguage_DIR="${SYSTEMC_HOME}/lib/cmake/SystemCLanguage"
  -DSystemCCCI_DIR="${CCI_HOME}/lib/cmake/SystemCCCI"
  -DBoost_NO_SYSTEM_PATHS=ON -DBoost_NO_BOOST_CMAKE=ON
  -DCMAKE_BUILD_RPATH="${OPENSSL_LIB};${BOOST_LIB};${VP_SYSTEMC_LIB};${VP_CCI_LIB}"
  -DCMAKE_INSTALL_RPATH="${OPENSSL_LIB};${BOOST_LIB};${VP_SYSTEMC_LIB};${VP_CCI_LIB}"
)

mkdir -p "${BUILD_DIR}"
if ((${#CMAKE_EXTRA[@]})); then
  cmake "${CMAKE_ARGS[@]}" "${CMAKE_EXTRA[@]}"
else
  cmake "${CMAKE_ARGS[@]}"
fi
echo "Configured ${BUILD_DIR}"
echo " cd build && make sep-vp"
