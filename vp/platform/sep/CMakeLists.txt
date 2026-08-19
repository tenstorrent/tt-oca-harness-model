# =============================================================================
# vp/platform/sep/CMakeLists.txt
#
# SEP platform VP target — same directory shape as vp/platform/smc/:
#   main.cpp + sep_platform.hpp + src/sep_platform.cpp + inc/ + config/ + docs/
# Builds the `sep-vp` executable.
# =============================================================================

# =============================================================================
# Add external models build tree
# =============================================================================
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../sep ${CMAKE_CURRENT_BINARY_DIR}/sep/peripherals)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../infra ${CMAKE_CURRENT_BINARY_DIR}/platform/infra)  # ← ADD THIS

find_package(OpenSSL REQUIRED)


# =============================================================================
# Include dirs for sep-vp main.cpp (Accellera only)
# =============================================================================
  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/hmac/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/otbn/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/spi_controller/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/spi_flash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/kmac/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/csrng/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/aes/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/mailbox/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/aon_timer/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/secure_dma/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/key_manager/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/efuse/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/entropy_src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/edn/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/sep_scratch_cold/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/sep_scratch_warm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/sep_output_remap_ctrl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/sep_filter_ctrl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/sep_reset_ctrl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/sep_cpu_ctrl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/local_master_alias_remap_ctrl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/cpu/VeeR-ISSTlm/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/utils/tlm_extensions
  )
  # Vendored ISS and softfloat: SYSTEM so their headers stay out of our warnings.
  include_directories(SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/cpu/VeeR-ISS/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/cpu/VeeR-ISS/softfloat/source/include
  )
  if(CCI_FOUND)
    include_directories(SYSTEM ${CCI_INCLUDE_DIR})
  endif()

  add_executable(sep-vp
    main.cpp
    src/sep_platform.cpp
  )

  target_compile_options(sep-vp PRIVATE -ftemplate-depth=8192)

  target_link_libraries(sep-vp
    platform-infra
    Boost::iostreams
    Boost::program_options
    SystemC::systemc
    Threads::Threads
    hmac_model
    otbn_model
    sep_memory_model
    sep_scratch_cold_model
    sep_scratch_warm_model
    sep_output_remap_ctrl_model
    sep_filter_ctrl_model
    sep_reset_ctrl_model
    sep_cpu_ctrl_model
    local_master_alias_remap_ctrl_model
    spi_controller_model
    spi_flash_model
    kmac_model
    OpenSSL::Crypto
    csrng_model
    aes_model
    mailbox_model
    aon_timer_model
    secure_dma_model
    key_manager_model
    efuse_model
    lifecycle_ctrl_model
    entropy_src_model
    edn_model
    veeriss_model
    z
  )

  if(CCI_FOUND)
    target_link_libraries(sep-vp CCI::cci)
  endif()

  install(TARGETS sep-vp RUNTIME DESTINATION ./)
