# Header-only platform helpers (bus.h, tlm_map.h, …). No compiled sources.
add_library(platform-infra INTERFACE)

target_include_directories(platform-infra INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(platform-infra INTERFACE SystemC::systemc)
