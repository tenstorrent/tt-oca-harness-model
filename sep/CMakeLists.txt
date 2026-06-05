cmake_minimum_required(VERSION 3.14)

project(sep_models LANGUAGES CXX)

message(STATUS "==========================================================")
message(STATUS "Building SEP Models")
message(STATUS "==========================================================")

# -----------------------------------------------------------------------------
# CMake Module Path
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# -----------------------------------------------------------------------------
# Build Options
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build model test executables" OFF)

if(BUILD_TESTS)
  message(STATUS "BUILD_TESTS=ON - Building model test executables")
else()
  message(STATUS "BUILD_TESTS=OFF - Building only model libraries (for VP integration)")
endif()

# -----------------------------------------------------------------------------
# CSML Utility Library
# -----------------------------------------------------------------------------
message(STATUS "Configuring CSML utility library...")
add_subdirectory(utils/csml)

# -----------------------------------------------------------------------------
# Peripheral Models
# Ordered so dependencies come before dependents.
# Each entry is skipped gracefully if its CMakeLists.txt is absent.
# -----------------------------------------------------------------------------
message(STATUS "Configuring generic models...")

set(_SEP_GENERIC
  uart_16550
  sep_memory
  rom
)

message(STATUS "Configuring OpenTitan models...")

set(_SEP_OT
  gpio
  hmac
  kmac
  otbn
  spi_controller
  spi_flash
  csrng
  aes
  secure_dma
  mailbox
  aon_timer
  key_manager
  efuse
  lifecycle_ctrl
  AVBbus
  entropy_src
  edn
  cpu
  cpu/VeeR-ISS/softfloat
)

foreach(_p IN LISTS _SEP_GENERIC _SEP_OT)
  set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/${_p}")
  if(EXISTS "${_dir}/CMakeLists.txt")
    add_subdirectory("${_dir}")
  else()
    message(STATUS "${_p}: No CMakeLists.txt found, skipping")
  endif()
endforeach()
unset(_dir)

message(STATUS "==========================================================")
message(STATUS "SEP Models configuration complete")
message(STATUS "==========================================================")
