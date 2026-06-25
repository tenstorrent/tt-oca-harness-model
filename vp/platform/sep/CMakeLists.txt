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
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/uart_16550/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/gpio/include
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/AVBbus/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/entropy_src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/peripherals/edn/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/cpu/VeeR-ISS/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/cpu/VeeR-ISS/softfloat/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sep/cpu/VeeR-ISSTlm/model/inc
  )
  if(CCI_FOUND)
    include_directories(${CCI_INCLUDE_DIR})
  endif()

  file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

  add_executable(sep-vp
    main.cpp
    ${HEADERS}
  )

  target_compile_options(sep-vp PRIVATE -ftemplate-depth=8192)

  target_link_libraries(sep-vp
    platform-infra
    Boost::iostreams
    Boost::program_options
    SystemC::systemc
    Threads::Threads
    uart_model
    gpio_model
    hmac_model
    otbn_model
    sep_memory_model
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
    avbbus_model
    entropy_src_model
    edn_model
    veeriss_model
    z
  )

  if(CCI_FOUND)
    target_link_libraries(sep-vp CCI::cci)
  endif()

  install(TARGETS sep-vp RUNTIME DESTINATION ./)
