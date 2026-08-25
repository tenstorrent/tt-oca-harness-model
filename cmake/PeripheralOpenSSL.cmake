# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Pin OpenSSL to OPENSSL_ROOT_DIR / ENV{OPENSSL_ROOT}.
# On macOS, find_package(OpenSSL) often picks Homebrew via pkg-config instead.
#
# When OPENSSL_ROOT is a from-source install this must cope with:
#   * shared builds        (lib/lib64 + .so/.dylib)
#   * static-only builds   (no-shared → only .a, e.g. the RHEL 8 CI OpenSSL)
#   * lib vs lib64 vs lib/<arch> layouts
# It always points the OPENSSL_*_LIBRARY cache vars at a file that exists so
# that find_package(OpenSSL) creates the OpenSSL::SSL / OpenSSL::Crypto
# imported targets; otherwise CMake reports OPENSSL_FOUND=TRUE but silently
# skips target creation, and every `target_link_libraries(... OpenSSL::SSL)`
# fails with "target not found".

# Locate one OpenSSL library (shared preferred, static fallback) under a root.
# Sets ${OUT_VAR} in the caller scope to the first existing file, or "" if none.
function(_peripheral_openssl_pick_lib OUT_VAR ROOT NAME)
  set(_candidates)
  if(APPLE)
    list(APPEND _candidates
      "${ROOT}/lib/lib${NAME}.dylib"
      "${ROOT}/lib64/lib${NAME}.dylib")
  endif()
  list(APPEND _candidates
    "${ROOT}/lib64/lib${NAME}.so"
    "${ROOT}/lib/lib${NAME}.so")
  if(CMAKE_LIBRARY_ARCHITECTURE)
    list(APPEND _candidates
      "${ROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/lib${NAME}.so")
  endif()
  # Static fallbacks (e.g. OpenSSL built with `no-shared`).
  list(APPEND _candidates
    "${ROOT}/lib64/lib${NAME}.a"
    "${ROOT}/lib/lib${NAME}.a")
  if(CMAKE_LIBRARY_ARCHITECTURE)
    list(APPEND _candidates
      "${ROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/lib${NAME}.a")
  endif()

  set(_found "")
  foreach(_cand IN LISTS _candidates)
    if(EXISTS "${_cand}")
      set(_found "${_cand}")
      break()
    endif()
  endforeach()
  set(${OUT_VAR} "${_found}" PARENT_SCOPE)
endfunction()

macro(peripheral_find_openssl)
  if(NOT OPENSSL_ROOT_DIR AND DEFINED ENV{OPENSSL_ROOT})
    set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT}")
  endif()

  if(OPENSSL_ROOT_DIR)
    set(_peripheral_ossl_root "${OPENSSL_ROOT_DIR}")
    if(NOT EXISTS "${_peripheral_ossl_root}/include/openssl/ssl.h")
      message(FATAL_ERROR "OpenSSL headers not found under OPENSSL_ROOT_DIR=${_peripheral_ossl_root}")
    endif()
    set(OPENSSL_INCLUDE_DIR "${_peripheral_ossl_root}/include" CACHE PATH "" FORCE)

    _peripheral_openssl_pick_lib(_peripheral_ssl_lib    "${_peripheral_ossl_root}" ssl)
    _peripheral_openssl_pick_lib(_peripheral_crypto_lib "${_peripheral_ossl_root}" crypto)
    if(NOT _peripheral_ssl_lib OR NOT _peripheral_crypto_lib)
      message(FATAL_ERROR
        "OpenSSL libraries not found under ${_peripheral_ossl_root} "
        "(looked for lib/lib64 .so/.dylib/.a). ssl='${_peripheral_ssl_lib}' "
        "crypto='${_peripheral_crypto_lib}'")
    endif()
    set(OPENSSL_SSL_LIBRARY    "${_peripheral_ssl_lib}"    CACHE FILEPATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${_peripheral_crypto_lib}" CACHE FILEPATH "" FORCE)
  endif()

  find_package(OpenSSL REQUIRED)

  # Guarantee the imported targets exist. Some CMake versions report
  # OPENSSL_FOUND=TRUE without creating OpenSSL::SSL/OpenSSL::Crypto when the
  # library variables were pre-seeded (especially for static/no-shared installs).
  if(OPENSSL_FOUND)
    if(NOT TARGET OpenSSL::Crypto AND OPENSSL_CRYPTO_LIBRARY)
      add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    endif()
    if(NOT TARGET OpenSSL::SSL AND OPENSSL_SSL_LIBRARY)
      add_library(OpenSSL::SSL UNKNOWN IMPORTED)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
      if(TARGET OpenSSL::Crypto)
        set_target_properties(OpenSSL::SSL PROPERTIES
          INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
      endif()
    endif()

    # A static (no-shared) libcrypto.a pulls dlopen/dlsym/dladdr from libdl and
    # pthread symbols; those system deps are not always attached to the imported
    # target (depends on CMake version / how the target was created). Attach them
    # explicitly so consumers link cleanly. Harmless for shared builds and no-op
    # on macOS (CMAKE_DL_LIBS is empty there).
    if(TARGET OpenSSL::Crypto AND OPENSSL_CRYPTO_LIBRARY MATCHES "\\.a$")
      find_package(Threads QUIET)
      if(CMAKE_DL_LIBS)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
          INTERFACE_LINK_LIBRARIES ${CMAKE_DL_LIBS})
      endif()
      if(TARGET Threads::Threads)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
          INTERFACE_LINK_LIBRARIES Threads::Threads)
      endif()
    endif()
  endif()
endmacro()
