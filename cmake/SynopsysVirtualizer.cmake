# Synopsys Virtualizer stack for CMake — aligned with vayavyalabs/Makefile and
# tenstorrent_sep/Makefile (VIRTUALIZER_ROOT, lib-gcc-9.5-64, CWR_SCO2 link cluster).
#
# Defines:
#   Snps::VirtualizerRuntime — INTERFACE: includes, compile defs, link dirs/libs, rpath
#   SystemC::systemc        — INTERFACE IMPORTED (links Snps runtime; Accellera FindSystemC not used)
#   vp::sysc_backend        — set by parent to depend on Snps::VirtualizerRuntime
#
# Cache:
#   SNPS_VP_ROOT     — default: /tools_vendor/synopsys/virtualizer-tool-elite/V-2024.03/SLS/linux
#   SNPS_VP_GCC_TAG  — default: gcc-9.5-64 (matches lib-gcc-9.5-64 / libso-gcc-9.5-64)

if(TARGET Snps::VirtualizerRuntime)
  return()
endif()

if(NOT DEFINED SNPS_VP_ROOT)
  if(DEFINED ENV{SNPS_VP_ROOT})
    set(SNPS_VP_ROOT "$ENV{SNPS_VP_ROOT}")
  else()
    message(FATAL_ERROR "SNPS_VP_ROOT is not set. Please export SNPS_VP_ROOT in your environment (e.g. by sourcing set_snps_third_party_env.sh) and re-run CMake.")
  endif()
endif()
set(SNPS_VP_GCC_TAG "gcc-9.5-64" CACHE STRING "Toolchain tag for lib-gcc-* / libso-* / gnu/*/lib64")

if(NOT EXISTS "${SNPS_VP_ROOT}/common/include/systemc.h")
  if(NOT EXISTS "${SNPS_VP_ROOT}/common/include")
    message(WARNING "SNPS_VP_ROOT may be wrong: ${SNPS_VP_ROOT}")
  endif()
endif()

get_filename_component(_SNPS_IP_ROOT "${SNPS_VP_ROOT}/../../IP" ABSOLUTE)

set(_SNPS_LIB "lib-${SNPS_VP_GCC_TAG}")
set(_SNPS_LIBSO "libso-${SNPS_VP_GCC_TAG}")
set(_SNPS_GNU_LIB64 "${SNPS_VP_ROOT}/gnu/${SNPS_VP_GCC_TAG}/lib64")

add_library(Snps::VirtualizerRuntime INTERFACE IMPORTED GLOBAL)

set_property(TARGET Snps::VirtualizerRuntime PROPERTY INTERFACE_INCLUDE_DIRECTORIES
  "${SNPS_VP_ROOT}/common/include"
  "${SNPS_VP_ROOT}/common/include/tlm"
  "${SNPS_VP_ROOT}/common/include/tlm/tlm_utils"
  "${SNPS_VP_ROOT}/common/include/uvmc-2.3.1"
  "${SNPS_VP_ROOT}/common/include/tlm_serial"
  "${_SNPS_IP_ROOT}/tlm_ft_can_bus/SystemC/include"
  "${_SNPS_IP_ROOT}/tlm_ft_spi_bus/SystemC/include"
  "${_SNPS_IP_ROOT}/tlm_ft_lin_bus/SystemC/include"
  "${SNPS_VP_ROOT}/tlmcreator/templates/models/common/include"
  "${SNPS_VP_ROOT}/common/include/scml2_protocol_engines/tlm2_ft_target_port/include"
  "${SNPS_VP_ROOT}/common/include/scml2_protocol_engines/tlm2_ft_initiator_port/include"
  "${SNPS_VP_ROOT}/common/include"
  "${SNPS_VP_ROOT}/common/include/tlm"
)

set_property(TARGET Snps::VirtualizerRuntime PROPERTY INTERFACE_COMPILE_DEFINITIONS
  SNPS_SCML_LIB
  USE_CSML_REG_LIB
  SC_ALLOW_DEPRECATED_IEEE_API
  SC_INCLUDE_DYNAMIC_PROCESSES
  MEM_CALLBACKS
)

# tenstorrent_sep/Makefile: OCH_SEP_BUILD_FLAGS := -ftemplate-depth=4096 -DMEM_CALLBACKS
set_property(TARGET Snps::VirtualizerRuntime PROPERTY INTERFACE_COMPILE_OPTIONS
  -fPIC
  -ftemplate-depth=4096
)

# Match Makefile VP_RPATH + library search paths (vayavyalabs/Makefile / tenstorrent_sep/Makefile)
set_property(TARGET Snps::VirtualizerRuntime PROPERTY INTERFACE_LINK_DIRECTORIES
  "${SNPS_VP_ROOT}/common/${_SNPS_LIB}"
  "${SNPS_VP_ROOT}/sd/coware/${_SNPS_LIB}"
  "${SNPS_VP_ROOT}/common/lib-any-64"
  "${SNPS_VP_ROOT}/common/${_SNPS_LIBSO}"
)

# Rpath + CWR_SCO2-style -u flags (Makefile: -usc_main_hook -uSnpsVPExtLinkHook)
set_property(TARGET Snps::VirtualizerRuntime PROPERTY INTERFACE_LINK_OPTIONS
  "LINKER:-rpath,${_SNPS_GNU_LIB64}"
  "LINKER:-rpath,${SNPS_VP_ROOT}/common/${_SNPS_LIBSO}"
  "LINKER:--export-dynamic"
  "LINKER:-u,sc_main_hook"
  "LINKER:-u,SnpsVPExtLinkHook"
)

set_property(TARGET Snps::VirtualizerRuntime PROPERTY INTERFACE_LINK_LIBRARIES
  sc_main
  SnpsVPExt
  SnpsVPFt
  SnpsVP
  tbb
  omniORB4
  omniDynamic4
  omnithread
  pthread
  dwarf
  elf
  rt
  dl
)

# SystemC from Snps bundle (models use target_link_libraries(... SystemC::systemc))
add_library(SystemC::systemc INTERFACE IMPORTED GLOBAL)
set_property(TARGET SystemC::systemc PROPERTY INTERFACE_LINK_LIBRARIES Snps::VirtualizerRuntime)

if(NOT TARGET systemc)
  add_library(systemc ALIAS SystemC::systemc)
endif()
