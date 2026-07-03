# Derived paths from SYSTEMC_HOME, CCI_HOME, OPENSSL_ROOT, BOOST_ROOT.
# Set those four in configure_vp.sh (or export them before sourcing).

vp_libdir() {
  local root="$1"
  if [[ "$(uname -s)" == "Darwin" ]]; then
    [[ -d "${root}/lib" ]] && echo "${root}/lib" && return
  else
    [[ -d "${root}/lib-linux64" ]] && echo "${root}/lib-linux64" && return
    [[ -d "${root}/lib64" ]] && echo "${root}/lib64" && return
    [[ -d "${root}/lib" ]] && echo "${root}/lib" && return
  fi
  echo "${root}/lib"
}

vp_export_build_paths() {
  local missing=0 v val
  for v in SYSTEMC_HOME CCI_HOME OPENSSL_ROOT BOOST_ROOT; do
    eval "val=\${${v}:-}"
    if [[ -z "$val" ]]; then
      echo "vp_build_env: ${v} is not set (edit configure_vp.sh or export ${v})" >&2
      missing=1
    fi
  done
  (( missing == 0 )) || return 1

  export VP_HOST_OS="$(uname -s)"
  export OPENSSL_ROOT_DIR="${OPENSSL_ROOT}"
  export BOOST_INC="${BOOST_ROOT}/include"
  export BOOST_LIB="$(vp_libdir "${BOOST_ROOT}")"
  export OPENSSL_INC="${OPENSSL_ROOT}/include"
  export OPENSSL_LIB="$(vp_libdir "${OPENSSL_ROOT}")"
  export CRYPTO_LIB="${OPENSSL_LIB}"
  export CXX_STD="c++${CMAKE_CXX_STANDARD:-20}"
  export VP_SYSTEMC_LIB="$(vp_libdir "${SYSTEMC_HOME}")"
  export VP_CCI_LIB="$(vp_libdir "${CCI_HOME}")"
  export CMAKE_PREFIX_PATH="${SYSTEMC_HOME};${CCI_HOME};${BOOST_ROOT};${OPENSSL_ROOT}"

  local runtime="${OPENSSL_LIB}:${BOOST_LIB}:${VP_SYSTEMC_LIB}:${VP_CCI_LIB}"
  if [[ "${VP_HOST_OS}" == "Darwin" ]]; then
    export DYLD_LIBRARY_PATH="${runtime}"
  else
    export LD_LIBRARY_PATH="${runtime}"
  fi
  export PATH="${OPENSSL_ROOT}/bin:${BOOST_ROOT}/bin:${PATH}"
  return 0
}

vp_print_build_env() {
  echo "VP_HOST_OS=${VP_HOST_OS}"
  echo "SYSTEMC_HOME=${SYSTEMC_HOME}"
  echo "CCI_HOME=${CCI_HOME}"
  echo "OPENSSL_ROOT=${OPENSSL_ROOT}"
  echo "BOOST_ROOT=${BOOST_ROOT}"
  echo "CXX_STD=${CXX_STD}"
  echo "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug} CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD:-20}"
  echo "OPENSSL_LIB=${OPENSSL_LIB}"
  echo "BOOST_LIB=${BOOST_LIB}"
  echo "SYSTEMC_LIB=${VP_SYSTEMC_LIB}"
  echo "CCI_LIB=${VP_CCI_LIB}"
  if [[ "${VP_HOST_OS}" == "Darwin" ]]; then
    echo "DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH}"
  else
    echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
  fi
}
