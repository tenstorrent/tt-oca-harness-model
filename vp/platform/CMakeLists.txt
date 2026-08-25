# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Only one platform target exists; list it explicitly so CMake
# re-evaluates when files change (GLOB does not trigger re-configure).
add_subdirectory(sep)

# The SMC platform requires the Whisper ISS (https://github.com/tenstorrent/whisper).  Skip it
# when WHISPER_HOME is unavailable so that SEP-only CI builds configure cleanly.
if(DEFINED ENV{WHISPER_HOME} AND NOT "$ENV{WHISPER_HOME}" STREQUAL "")
    add_subdirectory(smc)
    # The SMU platform integrates the SMC and SEP platforms (needs both).
    add_subdirectory(smu)
else()
    message(STATUS "WHISPER_HOME not set; skipping smc and smu platforms in this build")
endif()
