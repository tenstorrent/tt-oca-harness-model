# Fallback when this subdirectory is configured without the top-level vp/CMakeLists.txt:
# define vp::sysc_backend as Accellera + CCI only.
#
# Synopsys Virtualizer (SNPS_SCML_LIB, Snps VP link line) is not configured here;
# use vayavyalabs/tenstorrent_sep/Makefile and vayavyalabs/vs_wk/OCH_SEP_SS_SNPS/Makefile.

if(TARGET vp::sysc_backend)
  return()
endif()

add_library(vp::sysc_backend INTERFACE IMPORTED GLOBAL)

set(ACCELLERA_CCI_STD 1 CACHE BOOL "Build with Accellera CCI" FORCE)

set_property(TARGET vp::sysc_backend PROPERTY
  INTERFACE_COMPILE_DEFINITIONS ACCELLERA_CCI_STD USE_CCI_CONFIG
)
