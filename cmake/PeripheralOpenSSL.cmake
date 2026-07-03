# Pin OpenSSL to OPENSSL_ROOT_DIR / ENV{OPENSSL_ROOT}.
# On macOS, find_package(OpenSSL) often picks Homebrew via pkg-config instead.

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
    if(APPLE)
      set(OPENSSL_SSL_LIBRARY "${_peripheral_ossl_root}/lib/libssl.dylib" CACHE FILEPATH "" FORCE)
      set(OPENSSL_CRYPTO_LIBRARY "${_peripheral_ossl_root}/lib/libcrypto.dylib" CACHE FILEPATH "" FORCE)
    elseif(EXISTS "${_peripheral_ossl_root}/lib64/libssl.so")
      set(OPENSSL_SSL_LIBRARY "${_peripheral_ossl_root}/lib64/libssl.so" CACHE FILEPATH "" FORCE)
      set(OPENSSL_CRYPTO_LIBRARY "${_peripheral_ossl_root}/lib64/libcrypto.so" CACHE FILEPATH "" FORCE)
    elseif(CMAKE_LIBRARY_ARCHITECTURE AND
           EXISTS "${_peripheral_ossl_root}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libssl.so")
      set(OPENSSL_SSL_LIBRARY
          "${_peripheral_ossl_root}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libssl.so" CACHE FILEPATH "" FORCE)
      set(OPENSSL_CRYPTO_LIBRARY
          "${_peripheral_ossl_root}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libcrypto.so" CACHE FILEPATH "" FORCE)
    else()
      set(OPENSSL_SSL_LIBRARY "${_peripheral_ossl_root}/lib/libssl.so" CACHE FILEPATH "" FORCE)
      set(OPENSSL_CRYPTO_LIBRARY "${_peripheral_ossl_root}/lib/libcrypto.so" CACHE FILEPATH "" FORCE)
    endif()
  endif()

  find_package(OpenSSL REQUIRED)
endmacro()
