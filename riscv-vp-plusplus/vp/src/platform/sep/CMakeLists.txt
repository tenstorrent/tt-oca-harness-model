# =============================================================================
# Add external models build tree
# =============================================================================
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models ${CMAKE_CURRENT_BINARY_DIR}/models)

find_package(OpenSSL REQUIRED)

# Default to building the sep-vp executable (main.cpp). Synopsys Virtualizer flow
# will explicitly disable this after successfully detecting the OCH project.
set(_sep_use_main_cpp ON)

# =============================================================================
# Synopsys Virtualizer: optional OCH_SEP_SS_SNPS flow (vs_wk/OCH_SEP_SS_SNPS/Makefile)
# — SystemC/*.cc + Tests/Unittests/*.cc + libOCHSEPModels-equivalent static libs; main.cpp not used.
# =============================================================================
set(TENSTORRENT_SEP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." CACHE PATH
  "tenstorrent_sep root (includes; same tree as tenstorrent_sep/Makefile libOCHSEPModels.a).")
set(OCH_SEP_SS_SNPS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/OCH_SEP_SS_SNPS" CACHE PATH
  "Path to OCH_SEP_SS_SNPS Virtualizer project (SystemC/ + Tests/). When set with VP_SYSC_BACKEND=synopsys, builds sep-vp from SystemC/*.cc + Tests/Unittests/*.cc + merged libOCHSEPModels.a (no main.cpp).")

# If a build directory previously cached OCH_SEP_SS_SNPS_ROOT as empty, re-seed it
# to the in-tree default so users don't have to pass -D manually.
if(VP_SYSC_BACKEND STREQUAL "synopsys" AND OCH_SEP_SS_SNPS_ROOT STREQUAL "")
  set(OCH_SEP_SS_SNPS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/OCH_SEP_SS_SNPS" CACHE PATH
    "Path to OCH_SEP_SS_SNPS Virtualizer project (SystemC/ + Tests/). When set with VP_SYSC_BACKEND=synopsys, builds sep-vp from SystemC/*.cc + Tests/Unittests/*.cc + merged libOCHSEPModels.a (no main.cpp)." FORCE)
endif()

if(VP_SYSC_BACKEND STREQUAL "synopsys")
  if(OCH_SEP_SS_SNPS_ROOT
      AND EXISTS "${OCH_SEP_SS_SNPS_ROOT}/SystemC/src/OCH_SEP_SS_SNPS.cc"
      AND EXISTS "${OCH_SEP_SS_SNPS_ROOT}/SystemC/src/OCH_SEP_SS_SNPSBase.cc"
      AND EXISTS "${OCH_SEP_SS_SNPS_ROOT}/SystemC/src/OCH_SEP_SS_SNPS.dladapter.cc"
      AND EXISTS "${OCH_SEP_SS_SNPS_ROOT}/Tests/Unittests/OCH_SEP_SS_SNPSTest.cc")
    # Default: emit sep-vp and libOCH_SEP_SS_SNPS.so directly under the build tree
    # (${CMAKE_BINARY_DIR}), not under bin/ or lib/ (overrides vp/CMakeLists RUNTIME_OUTPUT_DIRECTORY).
    set(OCH_SEP_SNPS_ARTIFACT_DIR "${CMAKE_BINARY_DIR}" CACHE PATH
      "Output directory for sep-vp and libOCH_SEP_SS_SNPS.so (Synopsys OCH flow).")
    include(OchSepSnps)
    set(_sep_use_main_cpp OFF)
  else()
    message(STATUS "Synopsys: Virtualizer flow requires generated sources under ${OCH_SEP_SS_SNPS_ROOT}/SystemC/src (OCH_SEP_SS_SNPSBase.cc, OCH_SEP_SS_SNPS.dladapter.cc). Falling back to building sep-vp with main.cpp. If you intended the Virtualizer flow, generate the OCH_SEP_SS_SNPS project sources and/or set -DOCH_SEP_SS_SNPS_ROOT=<vs_wk/OCH_SEP_SS_SNPS> (see vs_wk/OCH_SEP_SS_SNPS/Makefile).")
  endif()
endif()

if(VP_SYSC_BACKEND STREQUAL "accellera")
	set(_sep_use_main_cpp ON)
endif()

# =============================================================================
# Add model include directories when compiling sep-vp main.cpp (Accellera or Synopsys fallback)
# =============================================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/generic/uart_16550/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/gpio/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/hmac/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/otbn/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/spi_controller/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/spi_flash/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/xspi_ctrl/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/kmac/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/crng/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/crng/csml/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/aes/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/mailbox/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/aon_timer/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/dma/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/crng/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/keymgr_tt/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/sep_efuse/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/AVBbus/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/entropy_src/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/ot/edn/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/veer-iss/VeeR-ISS/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/veer-iss/VeeR-ISS/softfloat/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../models/veer-iss/VeeR-ISSTlm/model/inc
)

if(VP_SYSC_BACKEND STREQUAL "accellera")
  if(CCI_FOUND)
    include_directories(${CCI_INCLUDE_DIR})
  endif()
endif()

# USE_CCI_CONFIG for Accellera comes from vp::sysc_backend (see vp/CMakeLists.txt).
# Makefile OCH_SEP_DEFINES: -DUSE_CSML_REG_LIB -DSC_ALLOW_DEPRECATED_IEEE_API (no USE_CCI_CONFIG for Synopsys).
add_compile_definitions(USE_CSML_REG_LIB)

if(_sep_use_main_cpp)
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(sep-vp
    main.cpp
	${HEADERS}
)

if(VP_SYSC_BACKEND STREQUAL "synopsys")
  target_compile_definitions(sep-vp PRIVATE ACCELLERA_CCI_STD=0)
else()
  target_compile_definitions(sep-vp PRIVATE ACCELLERA_CCI_STD=1)
endif()

target_compile_options(sep-vp PRIVATE -ftemplate-depth=4096)

target_link_libraries(sep-vp
    vp::sysc_backend
)

target_link_libraries(sep-vp
    softfloat
    platform-common
    ${Boost_LIBRARIES}
    SystemC::systemc
    pthread
    uart_model
    gpio_model
    hmac_model
    otbn_model
    sep_memory_model
    csml_logger
    spi_controller_model
    spi_flash_model
    xspi_flash_target
    xspi_ctrl_model
    kmac_model
    OpenSSL::Crypto
    crng_model
    aes_model
    mailbox_model
    aon_timer_model
    dma_model
    keymgr_tt_model
    sep_efuse_model
    lc_ctrl_model
    avbbus_model
    entropy_src_model
    edn_model
    veeriss_model
    -lz
    -lpthread
    -ldl
    -lboost_iostreams
    -lboost_program_options
    -lboost_system
)

if(VP_SYSC_BACKEND STREQUAL "accellera")
  if(CCI_FOUND)
    target_link_libraries(sep-vp CCI::cci)
  endif()
endif()

if(TARGET sep-vp)
  INSTALL(TARGETS sep-vp RUNTIME DESTINATION ./)
endif()
endif()

if(NOT _sep_use_main_cpp)
  if(TARGET sep-vp AND TARGET OCH_SEP_SS_SNPS)
    install(TARGETS sep-vp OCH_SEP_SS_SNPS
      RUNTIME DESTINATION ./
      LIBRARY DESTINATION ./
    )
  elseif(TARGET sep-vp)
    install(TARGETS sep-vp
      RUNTIME DESTINATION ./
    )
  elseif(TARGET OCH_SEP_SS_SNPS)
    install(TARGETS OCH_SEP_SS_SNPS
      LIBRARY DESTINATION ./
    )
  endif()
endif()

# Symlink libOCH_SEP_SS_SNPS.so into the directory above the build tree (parent of ${CMAKE_BINARY_DIR}).
if(NOT _sep_use_main_cpp AND TARGET OCH_SEP_SS_SNPS)
  get_filename_component(_och_sep_symlink_parent "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
  add_custom_command(TARGET OCH_SEP_SS_SNPS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f "${_och_sep_symlink_parent}/libOCH_SEP_SS_SNPS.so"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      $<TARGET_FILE:OCH_SEP_SS_SNPS>
      "${_och_sep_symlink_parent}/libOCH_SEP_SS_SNPS.so"
    COMMENT "Symlink libOCH_SEP_SS_SNPS.so -> ${_och_sep_symlink_parent}/"
  )
endif()
