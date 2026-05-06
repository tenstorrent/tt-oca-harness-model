# Synopsys Virtualizer flow (vs_wk/OCH_SEP_SS_SNPS/Makefile):
# 1) Merge CMake-built model static libs -> libOCHSEPModels.a
# 2) MODEL_LIB: libOCH_SEP_SS_SNPS.so from SystemC/src/*.cc + LINK_FLAGS (shared, no sc_main)
# 3) TARGET (sep-vp): Tests/Unittests/*.cc only + LDFLAGS (SEP_SHLIB + scml2 + CWR); Makefile does not link -lOCH_SEP_SS_SNPS into the exe (runtime load path)

if(NOT DEFINED SNPS_VP_ROOT)
  if(DEFINED ENV{SNPS_VP_ROOT})
    set(SNPS_VP_ROOT "$ENV{SNPS_VP_ROOT}")
  else()
    message(FATAL_ERROR "SNPS_VP_ROOT is not set. Please export SNPS_VP_ROOT in your environment (e.g. by sourcing set_snps_third_party_env.sh) and re-run CMake.")
  endif()
endif()

if(NOT SNPS_VP_GCC_TAG)
  set(SNPS_VP_GCC_TAG "gcc-9.5-64")
endif()

set(_och_sys "${OCH_SEP_SS_SNPS_ROOT}/SystemC")
set(_och_tst "${OCH_SEP_SS_SNPS_ROOT}/Tests")
set(_och_model_sources
  "${_och_sys}/src/OCH_SEP_SS_SNPS.cc"
  "${_och_sys}/src/OCH_SEP_SS_SNPSBase.cc"
  "${_och_sys}/src/OCH_SEP_SS_SNPS.dladapter.cc"
)
set(_och_test_sources "${_och_tst}/Unittests/OCH_SEP_SS_SNPSTest.cc")

set(_tsep "${TENSTORRENT_SEP_ROOT}")
set(_vp "${_tsep}/riscv-vp-plusplus/vp")
set(_snps "${SNPS_VP_ROOT}")
set(_snps_lib "lib-${SNPS_VP_GCC_TAG}")
set(_snps_libso "libso-${SNPS_VP_GCC_TAG}")
set(_gnu64 "${_snps}/gnu/${SNPS_VP_GCC_TAG}/lib64")
# Set by sep/CMakeLists.txt before include (default: CMAKE_BINARY_DIR); fallback matches old bin/lib layout.
if(DEFINED OCH_SEP_SNPS_ARTIFACT_DIR AND NOT OCH_SEP_SNPS_ARTIFACT_DIR STREQUAL "")
  set(_och_artifact_dir "${OCH_SEP_SNPS_ARTIFACT_DIR}")
else()
  set(_och_artifact_dir "${CMAKE_BINARY_DIR}/lib")
endif()

# Same static libs as tenstorrent_sep/Makefile libOCHSEPModels.a
set(_och_merge_libs
  softfloat
  platform-common
  uart_model
  gpio_model
  hmac_model
  otbn_model
  sep_memory_model
  csml_logger
  spi_controller_model
  i2c_model
  kmac_model
  crng_model
  aes_model
  veeriss_model
)

set(_merge_script "${CMAKE_CURRENT_LIST_DIR}/merge_static_archives.sh")
set(_merged_a "${CMAKE_CURRENT_BINARY_DIR}/libOCHSEPModels.a")

add_custom_command(
  OUTPUT "${_merged_a}"
  COMMAND bash "${_merge_script}" "${_merged_a}"
    $<TARGET_FILE:softfloat>
    $<TARGET_FILE:platform-common>
    $<TARGET_FILE:uart_model>
    $<TARGET_FILE:gpio_model>
    $<TARGET_FILE:hmac_model>
    $<TARGET_FILE:otbn_model>
    $<TARGET_FILE:sep_memory_model>
    $<TARGET_FILE:csml_logger>
    $<TARGET_FILE:spi_controller_model>
    $<TARGET_FILE:i2c_model>
    $<TARGET_FILE:kmac_model>
    $<TARGET_FILE:crng_model>
    $<TARGET_FILE:aes_model>
    $<TARGET_FILE:veeriss_model>
  DEPENDS ${_och_merge_libs}
  COMMENT "Merging model static libraries into libOCHSEPModels.a"
)

add_custom_target(merge_libochsep_models DEPENDS "${_merged_a}")

# Objects in libOCHSEPModels.a must be PIC to link into libOCH_SEP_SS_SNPS.so (Makefile MODEL_LIB).
foreach(_t IN LISTS _och_merge_libs)
  if(TARGET ${_t})
    get_property(_och_imp TARGET ${_t} PROPERTY IMPORTED)
    if(NOT _och_imp)
      set_property(TARGET ${_t} PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
  endif()
endforeach()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

find_library(SNPS_SCML2_TESTING_LIB scml2_testing
  PATHS "${_snps}/common/${_snps_lib}" "${_snps}/common/${_snps_libso}" NO_DEFAULT_PATH)
find_library(SNPS_SCML2_TESTING_MAIN_LIB scml2_testing_systemc_main
  PATHS "${_snps}/common/${_snps_lib}" "${_snps}/common/${_snps_libso}" NO_DEFAULT_PATH)
if(NOT SNPS_SCML2_TESTING_LIB OR NOT SNPS_SCML2_TESTING_MAIN_LIB)
  message(WARNING "scml2_testing libraries not found under ${_snps}/common/${_snps_lib}. Link may fail; see OCH_SEP_SS_SNPS/Makefile LDFLAGS.")
endif()

find_library(VNC_SERVER_LIB vncserver)
find_library(UTIL_LIB util)

# ---------------------------------------------------------------------------
# MODEL_LIB := libOCH_SEP_SS_SNPS.so — vs_wk Makefile $(MODEL_LIB): $(OBJECTS) $(SEP_LIB); $(CXX_LINK) $(OBJECTS) -o $@ $(LINK_FLAGS)
# LINK_FLAGS: -shared -O3 -fPIC, VP rpaths, Snps VP libs + SEP_SHLIB_LIBS (no sc_main / scml2_testing — those are for TARGET only)
# ---------------------------------------------------------------------------
add_library(OCH_SEP_SS_SNPS SHARED ${_och_model_sources})
add_dependencies(OCH_SEP_SS_SNPS merge_libochsep_models)
set_target_properties(OCH_SEP_SS_SNPS PROPERTIES
  OUTPUT_NAME "OCH_SEP_SS_SNPS"
  LIBRARY_OUTPUT_DIRECTORY "${_och_artifact_dir}"
)

get_target_property(_snps_inc Snps::VirtualizerRuntime INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_snps_defs Snps::VirtualizerRuntime INTERFACE_COMPILE_DEFINITIONS)
get_target_property(_snps_copts Snps::VirtualizerRuntime INTERFACE_COMPILE_OPTIONS)
target_include_directories(OCH_SEP_SS_SNPS PRIVATE ${_snps_inc})
target_compile_definitions(OCH_SEP_SS_SNPS PRIVATE ${_snps_defs})
target_compile_options(OCH_SEP_SS_SNPS PRIVATE ${_snps_copts})

target_include_directories(OCH_SEP_SS_SNPS PRIVATE
  "${_och_sys}/include"
  "${OCH_SEP_SS_SNPS_ROOT}"
  "${_snps}/tlmcreator/templates/models/common/include"
  "${_vp}/src/platform/common"
  "${_vp}/src/core/common"
  "${_tsep}/models/utils"
  "${_tsep}/models/utils/csml/inc"
  "${_tsep}/models/generic/sep_memory/model/inc"
  "${_tsep}/models/generic/uart_16550/model/inc"
  "${_tsep}/models/ot/aes/model/inc"
  "${_tsep}/models/ot/aon_timer/model/inc"
  "${_tsep}/models/ot/crng/model/inc"
  "${_tsep}/models/ot/dma/model/inc"
  "${_tsep}/models/ot/edn/model/inc"
  "${_tsep}/models/ot/gpio/model/inc"
  "${_tsep}/models/ot/hmac/model/inc"
  "${_tsep}/models/ot/i2c/model/inc"
  "${_tsep}/models/ot/kmac/model/inc"
  "${_tsep}/models/ot/lc_ctrl/model/inc"
  "${_tsep}/models/ot/mailbox/model/inc"
  "${_tsep}/models/ot/otbn/model/algo"
  "${_tsep}/models/ot/otbn/model/inc"
  "${_tsep}/models/ot/spi_controller/model/inc"
  "${_tsep}/models/veer-iss/VeeR-ISS"
  "${_tsep}/models/veer-iss/VeeR-ISSTlm/model/inc"
  "${_vp}/src/platform/sep"
  "${_vp}/src"
)

target_compile_definitions(OCH_SEP_SS_SNPS PRIVATE
  ACCELLERA_CCI_STD=0
  SC_INCLUDE_DYNAMIC_PROCESSES
  USE_CSML_REG_LIB
  SC_ALLOW_DEPRECATED_IEEE_API
)
# Makefile BUILD_FLAGS: -O3 -fvisibility=hidden -DNDEBUG for wrapper objects (Release)
target_compile_options(OCH_SEP_SS_SNPS PRIVATE
  -fmessage-length=0
  -Wno-deprecated
  -ftemplate-depth=4096
  $<$<CONFIG:Release>:-O3 -fvisibility=hidden -DNDEBUG>
)

target_link_directories(OCH_SEP_SS_SNPS PRIVATE
  "${_snps}/common/${_snps_lib}"
  "${_snps}/sd/coware/${_snps_lib}"
  "${_snps}/common/lib-any-64"
  "${_snps}/common/${_snps_libso}"
)

target_link_libraries(OCH_SEP_SS_SNPS PRIVATE
  "${_merged_a}"
  Boost::program_options
  Boost::iostreams
  OpenSSL::SSL
  OpenSSL::Crypto
  ZLIB::ZLIB
  stdc++fs
)
if(VNC_SERVER_LIB)
  target_link_libraries(OCH_SEP_SS_SNPS PRIVATE ${VNC_SERVER_LIB})
endif()
if(UTIL_LIB)
  target_link_libraries(OCH_SEP_SS_SNPS PRIVATE ${UTIL_LIB})
endif()
# Makefile LINK_FLAGS: -uSnpsVPExtLinkHook -lSnpsVPExt … (no -lsc_main / sc_main_hook)
target_link_libraries(OCH_SEP_SS_SNPS PRIVATE
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

target_link_options(OCH_SEP_SS_SNPS PRIVATE
  "LINKER:-u,SnpsVPExtLinkHook"
  "LINKER:--export-dynamic"
  "LINKER:--no-undefined"
  "LINKER:-rpath,${_gnu64}"
  "LINKER:-rpath,${_snps}/common/${_snps_libso}"
  "LINKER:-rpath,${_tsep}"
)
if(DEFINED Boost_LIBRARY_DIRS)
  foreach(_d IN LISTS Boost_LIBRARY_DIRS)
    target_link_options(OCH_SEP_SS_SNPS PRIVATE "LINKER:-rpath,${_d}")
  endforeach()
endif()
get_target_property(_och_so_crypto OpenSSL::Crypto IMPORTED_LOCATION)
if(_och_so_crypto AND NOT _och_so_crypto STREQUAL "IMPORTED_LOCATION-NOTFOUND")
  get_filename_component(_och_so_crypto_libdir "${_och_so_crypto}" DIRECTORY)
  target_link_options(OCH_SEP_SS_SNPS PRIVATE "LINKER:-rpath,${_och_so_crypto_libdir}")
endif()

# ---------------------------------------------------------------------------
# sep-vp = Makefile TARGET: $(TEST_OBJECTS) + LDFLAGS only (no explicit -lOCH_SEP_SS_SNPS)
# ---------------------------------------------------------------------------
add_executable(sep-vp ${_och_test_sources})
set_target_properties(sep-vp PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${_och_artifact_dir}"
)
add_dependencies(sep-vp merge_libochsep_models OCH_SEP_SS_SNPS)

target_include_directories(sep-vp PRIVATE
  "${_och_sys}/include"
  "${OCH_SEP_SS_SNPS_ROOT}"
  "${_snps}/tlmcreator/templates/models/common/include"
  "${_vp}/src/platform/common"
  "${_vp}/src/core/common"
  "${_tsep}/models/utils"
  "${_tsep}/models/utils/csml/inc"
  "${_tsep}/models/generic/sep_memory/model/inc"
  "${_tsep}/models/generic/uart_16550/model/inc"
  "${_tsep}/models/ot/aes/model/inc"
  "${_tsep}/models/ot/aon_timer/model/inc"
  "${_tsep}/models/ot/crng/model/inc"
  "${_tsep}/models/ot/dma/model/inc"
  "${_tsep}/models/ot/edn/model/inc"
  "${_tsep}/models/ot/gpio/model/inc"
  "${_tsep}/models/ot/hmac/model/inc"
  "${_tsep}/models/ot/i2c/model/inc"
  "${_tsep}/models/ot/kmac/model/inc"
  "${_tsep}/models/ot/lc_ctrl/model/inc"
  "${_tsep}/models/ot/mailbox/model/inc"
  "${_tsep}/models/ot/otbn/model/algo"
  "${_tsep}/models/ot/otbn/model/inc"
  "${_tsep}/models/ot/spi_controller/model/inc"
  "${_tsep}/models/veer-iss/VeeR-ISS"
  "${_tsep}/models/veer-iss/VeeR-ISSTlm/model/inc"
  "${_vp}/src/platform/sep"
  "${_vp}/src"
  "${_snps}/common/include/scml2_protocol_engines/tlm2_ft_target_port/include/testing"
  "${_snps}/common/include/scml2_protocol_engines/tlm2_ft_initiator_port/include/testing"
  "${_och_tst}/Unittests"
  "${_och_tst}"
)

target_compile_definitions(sep-vp PRIVATE
  ACCELLERA_CCI_STD=0
  SC_INCLUDE_DYNAMIC_PROCESSES
  USE_CSML_REG_LIB
  SC_ALLOW_DEPRECATED_IEEE_API
)
target_compile_options(sep-vp PRIVATE
  -fmessage-length=0
  -Wno-deprecated
  -ftemplate-depth=4096
)

target_link_libraries(sep-vp PRIVATE
  "${_merged_a}"
  Boost::program_options
  Boost::iostreams
  Boost::log
  OpenSSL::SSL
  OpenSSL::Crypto
  ZLIB::ZLIB
  stdc++fs
)
if(VNC_SERVER_LIB)
  target_link_libraries(sep-vp PRIVATE ${VNC_SERVER_LIB})
endif()
if(UTIL_LIB)
  target_link_libraries(sep-vp PRIVATE ${UTIL_LIB})
endif()
if(SNPS_SCML2_TESTING_MAIN_LIB)
  target_link_libraries(sep-vp PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,${SNPS_SCML2_TESTING_MAIN_LIB}>"
  )
endif()
if(SNPS_SCML2_TESTING_LIB)
  target_link_libraries(sep-vp PRIVATE ${SNPS_SCML2_TESTING_LIB})
endif()
target_link_libraries(sep-vp PRIVATE
  vp::sysc_backend
  Threads::Threads
)

target_link_options(sep-vp PRIVATE -m64)

target_link_options(sep-vp PRIVATE
  "LINKER:-rpath,${_gnu64}"
  "LINKER:-rpath,${_snps}/common/${_snps_libso}"
  "LINKER:-rpath,${_tsep}"
  "LINKER:-rpath,${_och_artifact_dir}"
)
if(DEFINED Boost_LIBRARY_DIRS)
  foreach(_d IN LISTS Boost_LIBRARY_DIRS)
    target_link_options(sep-vp PRIVATE "LINKER:-rpath,${_d}")
  endforeach()
endif()
get_target_property(_och_crypto OpenSSL::Crypto IMPORTED_LOCATION)
if(_och_crypto AND NOT _och_crypto STREQUAL "IMPORTED_LOCATION-NOTFOUND")
  get_filename_component(_och_crypto_libdir "${_och_crypto}" DIRECTORY)
  target_link_options(sep-vp PRIVATE "LINKER:-rpath,${_och_crypto_libdir}")
endif()

set_target_properties(sep-vp PROPERTIES
  BUILD_RPATH "${_och_artifact_dir}"
  INSTALL_RPATH "\$ORIGIN"
)

message(STATUS "OCH_SEP_SS_SNPS: libOCH_SEP_SS_SNPS.so and sep-vp -> ${_och_artifact_dir} (Unittests/*.cc + libOCHSEPModels.a + scml2 + Snps; main.cpp not used)")
