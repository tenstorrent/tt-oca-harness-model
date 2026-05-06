cmake_minimum_required(VERSION 3.22.0)
project(pka_model LANGUAGES C CXX)

# C++14: match SystemC and SCML (both built with cxx201402L)
set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard (match SystemC/SCML build)" FORCE)

# Option to select the SystemC environment (default: ACCELLERA_SYSTEMC)
set(SC_ENV "ACCELLERA_SYSTEMC" CACHE STRING "Select the SystemC environment: ACCELLERA_SYSTEMC, SYNOPSYS_SYSTEMC")
# Option to select the register access library (default: SCML)
set(REG_ACCESS "SCML" CACHE STRING "Select the register access library: SCML") 
# Option to select the parameter access library (default: SCML)
set(PARAM_ACCESS "SCML" CACHE STRING "Select the parameter access library: SCML or CCI")
# Option to select the DMI access 
set(DMI_ACCESS "DISABLE" CACHE STRING "Select the DMI access: ENABLE or DISABLE")

# Print the selected values
message(STATUS "Selected SystemC environment: ${SC_ENV}")
message(STATUS "Selected Register Access Library: ${REG_ACCESS}")
message(STATUS "Selected Parameter Access Library: ${PARAM_ACCESS}")
message(STATUS "Selected DMI Access: ${DMI_ACCESS}")

# Conditional logic based on SC_ENV
if(SC_ENV STREQUAL "ACCELLERA_SYSTEMC")
    message(STATUS "Using Accellera SystemC environment")
		set(IS_ACCELLERA_SYSTEMC 1 CACHE BOOL "Condition for SC_ENV being ACCELLERA_SYSTEMC" FORCE)
elseif(SC_ENV STREQUAL "SYNOPSYS_SYSTEMC")
    message(STATUS "Using SYNOPSYS_SYSTEMC environment")
		set(IS_SYNOPSYS_SYSTEMC 1 CACHE BOOL "Condition for SC_ENV being SYNOPSYS_SYSTEMC" FORCE)
		if(NOT REG_ACCESS STREQUAL "SCML")
			message(STATUS "For SC_ENV='SYNOPSYS_SYSTEMC', REG_ACCESS is automatically set to 'SCML'.")
			set(REG_ACCESS "SCML" CACHE STRING "Register Access Library set to 'SCML' for SC_ENV='SYNOPSYS_SYSTEMC'" FORCE)
		endif()
		if(NOT PARAM_ACCESS STREQUAL "SCML")
			message(STATUS "For SC_ENV='SYNOPSYS_SYSTEMC', PARAM_ACCESS is automatically set to 'SCML'.")
			set(PARAM_ACCESS "SCML" CACHE STRING "Parameter Access Library set to 'SCML' for SC_ENV='SYNOPSYS_SYSTEMC'" FORCE)
		endif()
		message(FATAL_ERROR "For SYNOPSYS_SYSTEMC do build in Synopsys Virtualizer")
else()
    message(FATAL_ERROR "Unknown SC_ENV value: ${SC_ENV}")
endif()

# Register access
if(REG_ACCESS STREQUAL "SCML")
    message(STATUS "Using Synopsys SCML Registers")
else()
    message(FATAL_ERROR "Unknown REG_ACCESS value: ${REG_ACCESS}")
endif()

# Parameter access 
if(PARAM_ACCESS STREQUAL "SCML")
    message(STATUS "Using Synopsys SCML Parameters")
elseif(PARAM_ACCESS STREQUAL "CCI")
    message(STATUS "Using Accellera CCI Parameters")
		set(CCI 1 CACHE BOOL "Condition for PARAM_ACCESS being CCI" FORCE)
else()
    message(FATAL_ERROR "Unknown REG_ACCESS value: ${REG_ACCESS}")
endif()

# DMI access
if(DMI_ACCESS STREQUAL "ENABLE")
    message(STATUS "DMI is ENABLED")
		set(DMI 1 CACHE BOOL "Condition for DMI_ACCESS being ENABLE" FORCE)
elseif(DMI_ACCESS STREQUAL "DISABLE")
    message(STATUS "DMI is DISABLED")
		set(DMI 0 CACHE BOOL "Condition for DMI_ACCESS being DISABLE" FORCE)
else()
    message(FATAL_ERROR "Unknown DMI_ACCESS value: ${DMI_ACCESS}")
endif()

# Specify the default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS " Build Type :  ${CMAKE_BUILD_TYPE}")
# Set output directories for release and debug builds
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_COVERAGE "${CMAKE_BINARY_DIR}/coverage")
# Conditionally set the output directory based on build type using generator expressions
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY 
    $<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/debug>
    $<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/release>
    $<$<CONFIG:Coverage>:${CMAKE_BINARY_DIR}/coverage>
)

# Set compiler flags for different build types
# Default
set(WFLAG "-g -Wall ")  
# Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
	add_compile_definitions(DEBUG)
	#enable only when needed to debug frame creation and its related APIs
  #add_compile_definitions(OMAC_UTIL_DEBUG)
endif()
# Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG) 
endif()
# Coverage builds
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
		message(STATUS " CMAKE_CXX_FLAGS :  ${CMAKE_CXX_FLAGS}")
endif()

#Add sources, include, libs etc
set(UT_PATH Tests/Unittests)                        

# Include directories for external libraries
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/SystemC/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tests/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tests/Unittests)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tests/Unittests/pka_driver/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Tests/Unittests/pka_driver/src/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../utils/logging/include/)

# Include SystemC 
include_directories($ENV{SYSTEMC_HOME}/include/) 

if(CCI)
include_directories($ENV{CCI_HOME}/include)
endif()

if(IS_ACCELLERA_SYSTEMC AND ((REG_ACCESS STREQUAL "SCML") OR (PARAM_ACCESS STREQUAL "SCML")))
message(STATUS "Including the SCML libraries")
include_directories($ENV{SCML_HOME}/include/)
# Add scmlinc include directory for SCML headers
include_directories($ENV{SCML_HOME}/src/ModelingObjects/scmlinc)
endif()

# Model sources
set(MODEL_PATH SystemC/src)
set(MODEL_SRCS
		${MODEL_PATH}/pka_model.cc
		${MODEL_PATH}/pka_modelBase.cc
)

# PKA driver C sources
set(PKA_DRIVER_SRCS
		${UT_PATH}/pka_driver/src/pka_driv.c
		${UT_PATH}/pka_driver/src/pka_core.c
		${UT_PATH}/pka_driver/src/pka_core_rsa.c
		${UT_PATH}/pka_driver/src/pka_core_ecc.c
		${UT_PATH}/pka_driver/src/pka_core_montg_ed.c
		${UT_PATH}/pka_driver/src/pka_rsa.c
		${UT_PATH}/pka_driver/src/pka_ecc.c
		${UT_PATH}/pka_driver/src/pka_montg_ed.c
		${UT_PATH}/pka_driver/src/pka_setup.c
		${UT_PATH}/pka_driver/src/asn1_der.c
		${UT_PATH}/pka_driver/src/sm3.c
		${UT_PATH}/pka_driver/src/sha512.c
		${UT_PATH}/pka_driver/src/ram_fw.c
)

# PKA sample app C sources
set(PKA_SAMPLE_APP_SRCS
		${UT_PATH}/pka_driver/sample_app/rsa_sign_verify_nist.c
		${UT_PATH}/pka_driver/sample_app/rsa_encrypt_decrypt.c
		${UT_PATH}/pka_driver/sample_app/ecc_sign_verify_asn1.c
		${UT_PATH}/pka_driver/sample_app/ecc_sign_verify_bp_p384r1.c
		${UT_PATH}/pka_driver/sample_app/ecc_encrypt_decrypt_bp_p256r1.c
		${UT_PATH}/pka_driver/sample_app/ecc_key_agree_nist384.c
		${UT_PATH}/pka_driver/sample_app/eddsa_sign_verify.c
		${UT_PATH}/pka_driver/sample_app/montg_key_agree.c
		${UT_PATH}/pka_driver/sample_app/sm2_sign_verify_256.c
		${UT_PATH}/pka_driver/sample_app/sm2_encrypt_decrypt_256.c
		${UT_PATH}/pka_driver/sample_app/sm2_key_agree_256.c
)

# Unittest sources
set(UT_SRCS
		${UT_PATH}/pka_modelTest.cc
		${UT_PATH}/pka_reg_wrappers.cc
)


# Add executable targets
add_executable(test_pka_model ${MODEL_SRCS} ${UT_SRCS} ${PKA_DRIVER_SRCS} ${PKA_SAMPLE_APP_SRCS})

# PKA driver: use STD_ERR so pka_debug.h uses fprintf(stderr) instead of virt_console.h
target_compile_definitions(test_pka_model PRIVATE STD_ERR)

# SystemC
if(IS_ACCELLERA_SYSTEMC)
	find_library(SYSTEMC_LIB systemc PATHS $ENV{SYSTEMC_HOME}/lib-linux64)
	target_compile_definitions(test_pka_model PRIVATE ACCELLERA_SYSTEMC)
endif()

#CCI
if(CCI)
	find_library(CCI_LIB cci PATHS $ENV{CCI_HOME}/lib-linux64)
	target_compile_definitions(test_pka_model PRIVATE USE_CCI_PARAMS)
endif()

#DMI
if(DMI)
	target_compile_definitions(test_pka_model PRIVATE DMI_ENABLE)
else()
	target_compile_definitions(test_pka_model PRIVATE DMI_DISABLE)
endif()

#Caching
if(ENABLE_CACHING)
	target_compile_definitions(test_pka_model PRIVATE ENABLE_CACHING=1)
else()
	target_compile_definitions(test_pka_model PRIVATE ENABLE_CACHING=0)
endif()

#SCML
if(IS_ACCELLERA_SYSTEMC AND ((REG_ACCESS STREQUAL "SCML") OR (PARAM_ACCESS STREQUAL "SCML")))
	find_library(SCML_LIB0 ModelingObjects PATHS $ENV{SCML_HOME}/lib-linux64) 
	find_library(SCML_LIB1 scml2					 PATHS $ENV{SCML_HOME}/lib-linux64)
	find_library(SCML_LIB2 scml2_base 		 PATHS $ENV{SCML_HOME}/lib-linux64)
	find_library(SCML_LIB3 scml2_logging 	 PATHS $ENV{SCML_HOME}/lib-linux64)
	find_library(SCML_LIB4 scml2_objects 	 PATHS $ENV{SCML_HOME}/lib-linux64)
	find_library(SCML_LIB5 scml2_tlm2      PATHS $ENV{SCML_HOME}/lib-linux64)
	find_library(SCML_LIB6 scml            PATHS $ENV{SCML_HOME}/lib-linux64)
	find_library(SCML_LIB7 scml_clock      PATHS $ENV{SCML_HOME}/lib-linux64)
endif()

# Find and link logging library (hybrid: package or in-tree fallback)
find_package(UtilsLogging QUIET)
if(UtilsLogging_FOUND)
  message(STATUS "UtilsLogging package found")
  set(UtilsLoggingTarget UtilsLogging::logging)
else()
  message(STATUS "UtilsLogging package not found; using in-tree utils/logging")
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../utils/logging ${CMAKE_BINARY_DIR}/utils_logging_build)
  set(UtilsLoggingTarget utils_logging)
endif()

# Link against external libraries
target_link_libraries(test_pka_model ${SYSTEMC_LIB} ${UtilsLoggingTarget}) 
 
if(IS_ACCELLERA_SYSTEMC AND ((REG_ACCESS STREQUAL "SCML") OR (PARAM_ACCESS STREQUAL "SCML")))
message(STATUS "LINKING THE SCML LIBRARIES")
target_link_libraries(test_pka_model ${SCML_LIB0} ${SCML_LIB1} ${SCML_LIB2} ${SCML_LIB3} ${SCML_LIB4} ${SCML_LIB5} ${SCML_LIB6} ${SCML_LIB7})
endif()

if(CCI)
message(STATUS "LINKING THE CCI LIBRARIES")
target_link_libraries(test_pka_model ${CCI_LIB})
endif()

if(REG_ACCESS STREQUAL "SCML")
  target_compile_definitions(test_pka_model PRIVATE USE_SCML_REG_LIB)
endif()
if(PARAM_ACCESS STREQUAL "SCML")
	target_compile_definitions(test_pka_model PRIVATE USE_SCML_PARAM_LIB)
endif()
if(IS_SYNOPSYS_SYSTEMC)
target_compile_definitions(test_pka_model PRIVATE SYNOPSYS_SYSTEMC)
endif()

# Custom target for generating code coverage reports using lcov
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
		set(RM_LIB '/usr/*' '$ENV{SYSTEMC_HOME}/include/*' '$ENV{PCAPPLUSPLUS_INCLUDE}/*' '$ENV{CCI_INCLUDE}/include/*' '$ENV{SCML_HOME}/include/*')
    # Create a custom target called "coverage"
    add_custom_target(coverage
				# Clean up previous coverage data
				COMMAND ./coverage/test_pka_model
        COMMAND lcov --capture --directory . --output-file coverage/coverage.info
        COMMAND lcov --remove coverage/coverage.info ${RM_LIB} --output-file=coverage/coverage_filtered.info
        COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
				COMMAND echo "Coverage report at coverage/html/index.html"
    )

    # Ensure the "coverage" target runs after "test_pka_model"
    add_dependencies(coverage test_pka_model)
endif()
