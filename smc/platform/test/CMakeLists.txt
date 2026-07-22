# Self-checking platform test benches.
#
# platform_tb        -- Phase 1: elaboration + MMIO round-trips (cluster-free).
# platform_fw_tb     -- Phase 2: firmware-on-cluster run (cluster-only).

# Firmware images (checked in) and their generator.  Regenerate via
#   python3 test/gen_firmware.py test/firmware.hex
#   python3 test/gen_firmware.py --park test/park.hex
set(SMC_PLATFORM_FIRMWARE_HEX_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    CACHE PATH "Directory containing firmware.hex / park.hex")
set(SMC_PLATFORM_FIRMWARE_HEX "${SMC_PLATFORM_FIRMWARE_HEX_DIR}/firmware.hex"
    CACHE FILEPATH "Path to the bootrom preload firmware hex image")
set(SMC_PLATFORM_PARK_HEX "${SMC_PLATFORM_FIRMWARE_HEX_DIR}/park.hex"
    CACHE FILEPATH "Path to the cluster-park firmware hex image")

add_executable(platform_tb platform_tb.cpp)
target_link_libraries(platform_tb PRIVATE smc_platform SystemC::cci)
target_compile_definitions(platform_tb PRIVATE
    SMC_PLATFORM_PARK_HEX="${SMC_PLATFORM_PARK_HEX}")
add_test(NAME platform_tb COMMAND platform_tb)
set_tests_properties(platform_tb PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "FAIL")

if(TARGET smc_cpu_cluster)
    add_executable(platform_fw_tb platform_fw_tb.cpp)
    target_link_libraries(platform_fw_tb PRIVATE smc_platform smc_cpu_cluster SystemC::cci)
    target_compile_definitions(platform_fw_tb PRIVATE
        SMC_PLATFORM_WITH_CLUSTER=1
        SMC_PLATFORM_FIRMWARE_HEX="${SMC_PLATFORM_FIRMWARE_HEX}")
    add_test(NAME platform_fw_tb COMMAND platform_fw_tb)
    set_tests_properties(platform_fw_tb PROPERTIES
        PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
        FAIL_REGULAR_EXPRESSION "FAIL")
endif()
