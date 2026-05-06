#!/usr/bin/env bash
# Source before Synopsys Virtualizer / PCT so OCH_SEP_SS_SNPS.tlmc resolves paths.
#
# Uses separate include vs lib directories (no required prefix layout):
#   BOOST_INC, BOOST_LIB
#   OPENSSL_INC, OPENSSL_LIB, CRYPTO_LIB  (CRYPTO_LIB is usually the same dir as OPENSSL_LIB)
#
# Set any of these before sourcing to override. Example single-prefix Boost:
#   export B=/path/to/boost-1.84.0
#   export BOOST_INC="$B/include" BOOST_LIB="$B/lib"
#
# Usage:   source /path/to/set_snps_third_party_env.sh

_default_sys_libdir="/usr/lib"
if [[ -d "/usr/lib/x86_64-linux-gnu" ]]; then
  _default_sys_libdir="/usr/lib/x86_64-linux-gnu"
fi

: "${BOOST_INC:=/usr/include}"
: "${BOOST_LIB:=${_default_sys_libdir}}"
: "${OPENSSL_INC:=/usr/include}"
: "${OPENSSL_LIB:=${_default_sys_libdir}}"
: "${CRYPTO_LIB:=${OPENSSL_LIB}}"
: "${SNPS_VP_ROOT:=/tools_vendor/synopsys/virtualizer-tool-elite/V-2024.03/SLS/linux}"

export BOOST_INC BOOST_LIB OPENSSL_INC OPENSSL_LIB CRYPTO_LIB SNPS_VP_ROOT

for _d in "${BOOST_INC}" "${BOOST_LIB}" "${OPENSSL_INC}" "${OPENSSL_LIB}" "${SNPS_VP_ROOT}"; do
  if [[ ! -d "$_d" ]]; then
    echo "set_snps_third_party_env.sh: warning: missing directory: $_d" >&2
  fi
done

echo "BOOST_INC=${BOOST_INC}"
echo "BOOST_LIB=${BOOST_LIB}"
echo "OPENSSL_INC=${OPENSSL_INC}"
echo "OPENSSL_LIB=${OPENSSL_LIB}"
echo "CRYPTO_LIB=${CRYPTO_LIB}"
echo "SNPS_VP_ROOT=${SNPS_VP_ROOT}"
