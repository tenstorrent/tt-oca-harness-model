# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Header-only platform helpers (bus.h, tlm_map.h, …). No compiled sources.
add_library(platform-infra INTERFACE)

target_include_directories(platform-infra INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(platform-infra INTERFACE SC_ALLOW_DEPRECATED_IEEE_API)

# SystemC is always required; vncserver is optional (VNC display).
find_library(VNCSERVER_LIB vncserver)
if(VNCSERVER_LIB)
    target_link_libraries(platform-infra INTERFACE SystemC::systemc ${VNCSERVER_LIB} util)
else()
    target_link_libraries(platform-infra INTERFACE SystemC::systemc util)
endif()
