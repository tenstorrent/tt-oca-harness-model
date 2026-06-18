# Header-only platform helpers (bus.h, tlm_map.h, …). No compiled sources.
add_library(platform-infra INTERFACE)

target_include_directories(platform-infra INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# systemc and util are always required; vncserver is optional (VNC display).
find_library(VNCSERVER_LIB vncserver)
if(VNCSERVER_LIB)
    target_link_libraries(platform-infra INTERFACE systemc ${VNCSERVER_LIB} util)
else()
    target_link_libraries(platform-infra INTERFACE systemc util)
endif()
