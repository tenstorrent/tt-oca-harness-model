cmake_minimum_required(VERSION 3.14)

project(sep_models LANGUAGES CXX)

message(STATUS "==========================================================")
message(STATUS "Building SEP Models")
message(STATUS "==========================================================")
if(VP_SYSC_BACKEND STREQUAL "synopsys")
  message(STATUS "SEP Models: VP_SYSC_BACKEND=synopsys — Accellera CCI / local SYSTEMC_HOME are not used (Synopsys SNPS_VP_ROOT + SystemC::systemc)")
endif()

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
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
# CSML Utility Library (Common SystemC Model Library)
# -----------------------------------------------------------------------------
message(STATUS "Configuring CSML utility library...")
add_subdirectory(utils/csml)

# -----------------------------------------------------------------------------
# Generic Models
# -----------------------------------------------------------------------------
message(STATUS "Configuring generic models...")

# UART 16550 (already has CMakeLists.txt)
add_subdirectory(generic/uart_16550)

# SEP Memory (check if CMakeLists.txt exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/generic/sep_memory/CMakeLists.txt)
  add_subdirectory(generic/sep_memory)
else()
  message(STATUS "SEP Memory: No CMakeLists.txt found, skipping")
endif()

# ROM (check if CMakeLists.txt exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/generic/rom/CMakeLists.txt)
  add_subdirectory(generic/rom)
else()
  message(STATUS "ROM: No CMakeLists.txt found, skipping")
endif()

# -----------------------------------------------------------------------------
# OpenTitan Models
# -----------------------------------------------------------------------------
message(STATUS "Configuring OpenTitan models...")

# GPIO (newly created CMakeLists.txt)
add_subdirectory(ot/gpio)

# HMAC (newly created CMakeLists.txt)
add_subdirectory(ot/hmac)

# KMAC
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/kmac/CMakeLists.txt)
  add_subdirectory(ot/kmac)
else()
  message(STATUS "KMAC: No CMakeLists.txt found, skipping")
endif()

# Other OT models (check if CMakeLists.txt exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/otbn/CMakeLists.txt)
  add_subdirectory(ot/otbn)
else()
  message(STATUS "OTBN: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/spi_controller/CMakeLists.txt)
  add_subdirectory(ot/spi_controller)
else()
  message(STATUS "SPI Controller: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/spi_flash/CMakeLists.txt)
  add_subdirectory(ot/spi_flash)
else()
  message(STATUS "SPI Flash: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/xspi_ctrl/CMakeLists.txt)
  add_subdirectory(ot/xspi_ctrl)
else()
  message(STATUS "xspi_ctrl: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/crng/CMakeLists.txt)
  add_subdirectory(ot/crng)
else()
  message(STATUS "CRNG: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/aes/CMakeLists.txt)
  add_subdirectory(ot/aes)
else()
  message(STATUS "AES: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/dma/CMakeLists.txt)
  add_subdirectory(ot/dma)
else()
  message(STATUS "DMA: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/mailbox/CMakeLists.txt)
  add_subdirectory(ot/mailbox)
else()
  message(STATUS "Mailbox: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/aon_timer/CMakeLists.txt)
  add_subdirectory(ot/aon_timer)
else()
  message(STATUS "AON Timer: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/keymgr_tt/CMakeLists.txt)
  add_subdirectory(ot/keymgr_tt)
else()
  message(STATUS "KeyMgr TT: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/sep_efuse/CMakeLists.txt)
  add_subdirectory(ot/sep_efuse)
else()
  message(STATUS "SEP eFuse: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/lc_ctrl/CMakeLists.txt)
  add_subdirectory(ot/lc_ctrl)
else()
  message(STATUS "Lifecycle Controller: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/AVBbus/CMakeLists.txt)
  add_subdirectory(ot/AVBbus)
else()
  message(STATUS "AVBbus: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/entropy_src/CMakeLists.txt)
  add_subdirectory(ot/entropy_src)
else()
  message(STATUS "Entropy Source: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ot/edn/CMakeLists.txt)
  add_subdirectory(ot/edn)
else()
  message(STATUS "EDN: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/veer-iss/CMakeLists.txt)
  add_subdirectory(veer-iss/)
else()
	message(STATUS "VEER_ISS: No CMakeLists.txt found, skipping")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/veer-iss/VeeR-ISS/softfloat/CMakeLists.txt)
	add_subdirectory(veer-iss/VeeR-ISS/softfloat)
else()
	message(STATUS "softfloat: No CMakeLists.txt found, skipping")
endif()

message(STATUS "==========================================================")
message(STATUS "SEP Models configuration complete")
message(STATUS "==========================================================")
