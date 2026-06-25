#!/usr/bin/env bash
# Configure and run CMake for the SEP Virtual Platform.
#
# USAGE
#   ./configure_vp.sh                          # auto-discover all paths
#   CMAKE_CXX_STANDARD=20 ./configure_vp.sh    # force C++20
#   SYSTEMC_HOME=/my/sc ./configure_vp.sh      # explicit SystemC path
#   source ./configure_vp.sh                   # export env without running cmake
#   ./configure_vp.sh -- -DFOO=BAR             # pass extra cmake args
#
# AUTO-DISCOVERY
#   If SYSTEMC_HOME, CCI_HOME, BOOST_ROOT, or OPENSSL_ROOT are not set the
#   script probes a list of well-known install prefixes and uses the first
#   match.  Set any variable explicitly to skip probing for that library.
#
# ENVIRONMENT VARIABLES (all optional — discovered if not set)
#   SYSTEMC_HOME        SystemC install prefix  (must contain include/systemc.h)
#   CCI_HOME            CCI install prefix      (must contain include/cci_configuration)
#   BOOST_ROOT          Boost install prefix    (must contain include/boost/version.hpp)
#   OPENSSL_ROOT        OpenSSL install prefix  (must contain include/openssl/ssl.h)
#   CMAKE_BUILD_TYPE    Debug | Release          (default: Debug)
#   CMAKE_CXX_STANDARD  17 | 20                 (default: 20)
#
# PLATFORM NOTES
#   macOS    : Homebrew paths are probed automatically.
#   Ubuntu   : System paths (/usr, /usr/local) are probed.  Install:
#                sudo apt install libboost-dev libssl-dev
#   RHEL 8   : Default GCC 8 does NOT support C++20.  Enable GCC 12 first:
#                scl enable gcc-toolset-12 bash
#              Then run this script.  The kmac peripheral requires OpenSSL ≥ 3.0
#              (RHEL 8 ships 1.1.1).  Build OpenSSL 3 from source or install
#              it to ~/local/openssl-3 and set OPENSSL_ROOT before running.

set -euo pipefail

VP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
: "${CMAKE_BUILD_TYPE:=Debug}"
: "${CMAKE_CXX_STANDARD:=20}"

[[ "${CMAKE_CXX_STANDARD}" =~ ^(17|20)$ ]] || {
  echo "error: CMAKE_CXX_STANDARD must be 17 or 20 (got '${CMAKE_CXX_STANDARD}')" >&2
  exit 1
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# find_prefix VARNAME "label" "probe-relative-path" candidate...
#   If VARNAME is already set and probe exists there, accept it.
#   Otherwise search candidates in order and take the first match.
#   Prints what was found; exits 1 with a clear message on failure.
find_prefix() {
  local varname="$1" label="$2" probe="$3"; shift 3
  local current="${!varname:-}"

  if [[ -n "${current}" ]]; then
    if [[ -e "${current}/${probe}" ]]; then
      printf '  %-18s = %s  (from environment)\n' "${varname}" "${current}"
      return 0
    else
      printf 'warning: %s="%s" set but "%s" not found there — probing anyway\n' \
        "${varname}" "${current}" "${probe}" >&2
    fi
  fi

  local candidate
  for candidate in "$@"; do
    if [[ -e "${candidate}/${probe}" ]]; then
      printf -v "${varname}" '%s' "${candidate}"
      printf '  %-18s = %s  (auto-discovered)\n' "${varname}" "${candidate}"
      return 0
    fi
  done

  echo "" >&2
  echo "error: cannot find ${label}." >&2
  printf '  Probe file : %s\n' "${probe}" >&2
  printf '  Searched   :\n' >&2
  for candidate in "$@"; do printf '    %s\n' "${candidate}"; done >&2
  echo "" >&2
  echo "  Fix: set ${varname}=/path/to/${label} and re-run." >&2
  return 1
}

# libdir PREFIX — resolves the right library subdirectory for this platform
libdir() {
  local p="$1"
  # SystemC/CCI on Linux (Accellera configure puts libs here)
  if   [[ -d "${p}/lib-linux64"              ]]; then echo "${p}/lib-linux64"
  # RHEL / Fedora / older distros
  elif [[ -d "${p}/lib64"                    ]]; then echo "${p}/lib64"
  # Debian/Ubuntu multiarch x86_64
  elif [[ -d "${p}/lib/x86_64-linux-gnu"    ]]; then echo "${p}/lib/x86_64-linux-gnu"
  # Debian/Ubuntu multiarch aarch64
  elif [[ -d "${p}/lib/aarch64-linux-gnu"   ]]; then echo "${p}/lib/aarch64-linux-gnu"
  # macOS Homebrew, manual installs, everything else
  else                                               echo "${p}/lib"
  fi
}

# ---------------------------------------------------------------------------
# Auto-discover dependencies
# ---------------------------------------------------------------------------
echo "Discovering dependencies..."

find_prefix SYSTEMC_HOME "SystemC" "include/systemc.h" \
  "${HOME}/local/systemc-3.0.2-cxx20" \
  "${HOME}/local/systemc-3.0.2-cxx17" \
  "${HOME}/local/systemc-3.0.2" \
  "${HOME}/local/systemc-3.0.1" \
  "${HOME}/local/systemc" \
  "${HOME}/systemc-3.0.2" \
  "${HOME}/systemc-3.0.1" \
  "${HOME}/systemc" \
  /usr/local/systemc-3.0.2 \
  /usr/local/systemc301 \
  /usr/local/systemc-3.0.1 \
  /usr/local/systemc3.0.1 \
  /usr/local/systemc \
  /usr/local \
  /usr/lib/systemc-3.0.2 \
  /usr/lib/systemc-3.0.1 \
  /usr/lib/systemc \
  /opt/homebrew/opt/systemc \
  /opt/homebrew/opt/libsystemc \
  /usr/local/opt/systemc \
  /usr/local/opt/libsystemc \
  /opt/local/libexec/systemc

find_prefix CCI_HOME "CCI" "include/cci_configuration" \
  "${HOME}/local/cci-1.0.2" \
  "${HOME}/local/cci-1.0.1" \
  "${HOME}/local/cci" \
  "${HOME}/cci-1.0.2" \
  "${HOME}/cci-1.0.1" \
  "${HOME}/cci" \
  /usr/local/cci-1.0.2 \
  /usr/local/cci-1.0.1 \
  /usr/local/cci \
  /usr/lib/cci-1.0.2 \
  /usr/lib/cci-1.0.1 \
  /usr/lib/cci \
  /opt/homebrew/opt/systemc-cci \
  /usr/local/opt/systemc-cci \
  /opt/local/libexec/cci

find_prefix BOOST_ROOT "Boost" "include/boost/version.hpp" \
  /usr \
  /usr/local \
  "${HOME}/local/boost" \
  /opt/homebrew/opt/boost \
  /opt/homebrew \
  /usr/local/opt/boost \
  /opt/local

find_prefix OPENSSL_ROOT "OpenSSL" "include/openssl/ssl.h" \
  "${HOME}/local/openssl-3.3.2" \
  "${HOME}/local/openssl-3.3" \
  "${HOME}/local/openssl-3.0" \
  "${HOME}/local/openssl-3" \
  "${HOME}/local/openssl" \
  /usr \
  /usr/local \
  "${HOME}/local/openssl" \
  /opt/homebrew/opt/openssl@3 \
  /opt/homebrew/opt/openssl \
  /usr/local/opt/openssl@3 \
  /usr/local/opt/openssl \
  /opt/local

echo ""

# ---------------------------------------------------------------------------
# GCC version guard: C++20 requires GCC 10 or later
# ---------------------------------------------------------------------------
if command -v g++ >/dev/null 2>&1; then
  _gxx_major=$(g++ -dumpversion 2>/dev/null | cut -d. -f1)
  if [[ "${CMAKE_CXX_STANDARD}" == "20" ]] && \
     [[ "${_gxx_major}" =~ ^[0-9]+$ ]] && \
     (( _gxx_major < 10 )); then
    echo "warning: g++ ${_gxx_major} detected; C++20 requires GCC ≥ 10." >&2
    echo "  RHEL 8 : scl enable gcc-toolset-12 bash && ./configure_vp.sh" >&2
    echo "  Ubuntu  : sudo apt install g++-12 && CXX=g++-12 ./configure_vp.sh" >&2
    echo "" >&2
  fi
fi

# ---------------------------------------------------------------------------
# Parse trailing cmake args
# ---------------------------------------------------------------------------
CMAKE_EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) sed -n '2,22p' "$0" | sed 's/^# \?//'; exit 0 ;;
    --) shift; CMAKE_EXTRA=("$@"); break ;;
    *) echo "error: unknown argument '$1'  (use VAR=value or -- cmake-args)" >&2; exit 1 ;;
  esac
done

# ---------------------------------------------------------------------------
# Export environment
# ---------------------------------------------------------------------------
export OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-${OPENSSL_ROOT}}"
export SYSTEMC_HOME CCI_HOME OPENSSL_ROOT BOOST_ROOT
export BOOST_INC="${BOOST_ROOT}/include"
# Use libdir() so BOOST_LIB resolves to lib64 on RHEL/Fedora, lib on others.
export BOOST_LIB="$(libdir "${BOOST_ROOT}")"
export OPENSSL_INC="${OPENSSL_ROOT}/include"
if [[ -d "${OPENSSL_ROOT_DIR}/lib64" ]]; then
  export OPENSSL_LIB="${OPENSSL_ROOT_DIR}/lib64"
else
  export OPENSSL_LIB="${OPENSSL_ROOT_DIR}/lib"
fi
export CRYPTO_LIB="${OPENSSL_LIB}"
export CMAKE_BUILD_TYPE CMAKE_CXX_STANDARD CXX_STD="c++${CMAKE_CXX_STANDARD}"

_sc_lib="$(libdir "${SYSTEMC_HOME}")"
_cci_lib="$(libdir "${CCI_HOME}")"
_boost_lib="$(libdir "${BOOST_ROOT}")"
_ssl_lib="${OPENSSL_LIB}"

export CMAKE_PREFIX_PATH="${SYSTEMC_HOME};${CCI_HOME};${BOOST_ROOT};${OPENSSL_ROOT_DIR}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}"

# RPATH: only non-system prefixes matter; include all to be safe
_rpaths="${_ssl_lib};${_boost_lib};${_sc_lib};${_cci_lib}"
export CMAKE_BUILD_RPATH="${_rpaths}"
export CMAKE_INSTALL_RPATH="${_rpaths}"

# Dynamic linker search path — macOS uses DYLD_LIBRARY_PATH, Linux uses LD_LIBRARY_PATH
_dynpath="${_ssl_lib}:${_boost_lib}:${_sc_lib}:${_cci_lib}"
if [[ "$(uname -s)" == "Darwin" ]]; then
  export DYLD_LIBRARY_PATH="${_dynpath}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
else
  export LD_LIBRARY_PATH="${_dynpath}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
fi

export PATH="${OPENSSL_ROOT}/bin:${BOOST_ROOT}/bin:${PATH}"

printf 'Build: type=%-10s  cxx=%s\n' "${CMAKE_BUILD_TYPE}" "${CMAKE_CXX_STANDARD}"

# If sourced (e.g. "source ./configure_vp.sh"), just export and return.
[[ "${BASH_SOURCE[0]}" != "${0}" ]] && return 0

# ---------------------------------------------------------------------------
# Run CMake
# ---------------------------------------------------------------------------
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
  -DSYSTEMC_HOME="${SYSTEMC_HOME}" -DCCI_HOME="${CCI_HOME}"
  -DBoost_NO_SYSTEM_PATHS=ON -DBoost_NO_BOOST_CMAKE=ON
  -DCMAKE_BUILD_RPATH="${_rpaths}"
  -DCMAKE_INSTALL_RPATH="${_rpaths}"
)

mkdir -p "${BUILD_DIR}"
cmake "${CMAKE_ARGS[@]}" ${CMAKE_EXTRA[@]+"${CMAKE_EXTRA[@]}"}

echo ""
echo "Configured: ${BUILD_DIR}"
echo "  cd $(basename "${BUILD_DIR}") && make sep-vp"
