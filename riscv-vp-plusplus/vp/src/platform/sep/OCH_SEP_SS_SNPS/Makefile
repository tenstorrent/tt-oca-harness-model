# OCH_SEP_SS_SNPS — Virtualizer wrapper + tenstorrent_sep (libOCHSEPModels)
# Run from this directory: make -f Makefile
# Requires: Synopsys Virtualizer (same layout as vayavyalabs/Makefile), optional Boost/OpenSSL paths below.

THIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OCH_SEP_ROOT := $(THIS_DIR)
# Repo root (tenstorrent_sep): OCH_SEP_SS_SNPS -> 6 parents = .../tenstorrent_sep (for includes)
TENSTORRENT_SEP ?= $(abspath $(OCH_SEP_ROOT)/../../../../../../..)

ifeq ($(strip $(SNPS_VP_ROOT)),)
$(error SNPS_VP_ROOT is not set. Please source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh (or export SNPS_VP_ROOT=/path/to/.../SLS/linux) and re-run make.)
endif()

ifeq ($(strip $(BOOST_INC)),)
$(error BOOST_INC is not set. Please source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh and re-run make.)
endif()
ifeq ($(strip $(BOOST_LIB)),)
$(error BOOST_LIB is not set. Please source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh and re-run make.)
endif()
ifeq ($(strip $(OPENSSL_INC)),)
$(error OPENSSL_INC is not set. Please source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh and re-run make.)
endif()
ifeq ($(strip $(OPENSSL_LIB)),)
$(error OPENSSL_LIB is not set. Please source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh and re-run make.)
endif()
ifeq ($(strip $(CRYPTO_LIB)),)
$(error CRYPTO_LIB is not set. Please source riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh and re-run make.)
endif()

# vpsession / scc live under common/bin (and tools/bin). Your login shell needs the same PATH — see below.
export PATH := $(SNPS_VP_ROOT)/common/bin:$(SNPS_VP_ROOT)/tools/bin:$(PATH)
export CWR_SYSTEMC_HOME := $(SNPS_VP_ROOT)/common/include

export CWR_SCO2_LDFLAGS := -L$(SNPS_VP_ROOT)/common/lib-gcc-9.5-64 \
	-L$(SNPS_VP_ROOT)/sd/coware/lib-gcc-9.5-64 \
	-L$(SNPS_VP_ROOT)/common/lib-any-64 \
	-usc_main_hook -lsc_main -uSnpsVPExtLinkHook -lSnpsVPExt -lSnpsVPFt -lSnpsVP -ltbb \
	-Wl,--export-dynamic \
	-L$(SNPS_VP_ROOT)/common/libso-gcc-9.5-64 \
	-lomniORB4 -lomniDynamic4 -lomnithread -lpthread -ldwarf -lelf -lrt -ldl

export LD_LIBRARY_PATH := $(SNPS_VP_ROOT)/gnu/gcc-9.5-64/lib64:$(SNPS_VP_ROOT)/common/libso-gcc-9.5-64:$(LD_LIBRARY_PATH)

CXX := scc +cxx $(SNPS_VP_ROOT)/common/bin/g++
CXX_LINK := $(SNPS_VP_ROOT)/common/bin/g++

EXTRA_LDFLAGS :=
COVERAGE_FLAGS ?= -fprofile-arcs -ftest-coverage

# Match system libstdc++ (GLIBCXX_3.4.26) + Snps shared libs + tenstorrent / boost / openssl
VP_RPATH := -Wl,-rpath,$(SNPS_VP_ROOT)/gnu/gcc-9.5-64/lib64 \
	-Wl,-rpath,$(SNPS_VP_ROOT)/common/libso-gcc-9.5-64

# Set by riscv-vp-plusplus/vp/src/platform/sep/set_snps_third_party_env.sh

# Directory containing CMake-built libOCHSEPModels.a (sibling of this OCH_SEP_SS_SNPS folder)
SEP_LIB_DIR := $(abspath $(OCH_SEP_ROOT)/..)
SEP_LIB := $(SEP_LIB_DIR)/libOCHSEPModels.a

VP_RPATH_EXTRA := -Wl,-rpath,$(SEP_LIB_DIR) -Wl,-rpath,$(TENSTORRENT_SEP) -Wl,-rpath,$(BOOST_LIB) -Wl,-rpath,$(OPENSSL_LIB) -Wl,-rpath,$(CRYPTO_LIB)

# Merged archive from CMake (VP_SYSC_BACKEND=synopsys): all model *.a -> libOCHSEPModels.a, copied to vp/src/platform/sep/

# Legacy: SEP_DEBUG / SEP_EXTRA_BUILD_FLAGS applied when building models via top-level tenstorrent_sep/Makefile (if present).
SEP_DEBUG ?= 1
ifeq ($(SEP_DEBUG),1)
SEP_EXTRA_BUILD_FLAGS := -g -O0 -fno-omit-frame-pointer -UNDEBUG
else
SEP_EXTRA_BUILD_FLAGS :=
endif


# --- Model / test outputs (gcc-9-5-64 tag matches Virtualizer lib dir) ---
GCC_TAG := gcc-9-5-64
MODEL_LIB := libOCH_SEP_SS_SNPS.so
TARGET := OCH_SEP_SS_SNPSTest.$(GCC_TAG)

# Wrapper + generated SystemC sources (under OCH_SEP_ROOT only)
SOURCES := $(OCH_SEP_ROOT)/SystemC/src/OCH_SEP_SS_SNPS.cc \
	$(OCH_SEP_ROOT)/SystemC/src/OCH_SEP_SS_SNPSBase.cc \
	$(OCH_SEP_ROOT)/SystemC/src/OCH_SEP_SS_SNPS.dladapter.cc

TEST_SOURCES := $(OCH_SEP_ROOT)/Tests/Unittests/OCH_SEP_SS_SNPSTest.cc

OBJECTS := $(SOURCES:.cc=.o)
TEST_OBJECTS := $(TEST_SOURCES:.cc=.o)

DEPS := $(OBJECTS:.o=.d)

# From OCH_SEP_SS_SNPS.tlmc / Makefile.OCH_SEP_SS_SNPS.linux
OCH_DEFINES := -DUSE_CSML_REG_LIB -DSC_ALLOW_DEPRECATED_IEEE_API

SEP_INCLUDES := \
	-I$(SNPS_VP_ROOT)/tlmcreator/templates/models/common/include \
	-I$(TENSTORRENT_SEP)/riscv-vp-plusplus/vp/src/platform/common \
	-I$(TENSTORRENT_SEP)/riscv-vp-plusplus/vp/src/core/common \
	-I$(TENSTORRENT_SEP)/models/utils \
	-I$(TENSTORRENT_SEP)/models/utils/csml/inc \
	-I$(TENSTORRENT_SEP)/models/generic/sep_memory/model/inc \
	-I$(TENSTORRENT_SEP)/models/generic/uart_16550/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/aes/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/aon_timer/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/crng/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/dma/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/edn/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/gpio/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/hmac/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/kmac/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/lc_ctrl/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/mailbox/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/otbn/model/algo \
	-I$(TENSTORRENT_SEP)/models/ot/otbn/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/spi_controller/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/spi_flash/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/xspi_ctrl/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/keymgr_tt/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/sep_efuse/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/AVBbus/model/inc \
	-I$(TENSTORRENT_SEP)/models/ot/entropy_src/model/inc \
	-I$(TENSTORRENT_SEP)/models/veer-iss/VeeR-ISS \
	-I$(TENSTORRENT_SEP)/models/veer-iss/VeeR-ISS/softfloat/source/include \
	-I$(TENSTORRENT_SEP)/models/veer-iss/VeeR-ISSTlm/model/inc \
	-I$(TENSTORRENT_SEP)/riscv-vp-plusplus/vp/src/platform/sep \
	-I$(TENSTORRENT_SEP)/riscv-vp-plusplus/vp/src \
	-I$(BOOST_INC) \
	-I$(OPENSSL_INC)

VP_BASE_INCLUDES := -I$(SNPS_VP_ROOT)/common/include \
	-I$(SNPS_VP_ROOT)/common/include/tlm \
	-I$(SNPS_VP_ROOT)/common/include/tlm/tlm_utils \
	-I$(SNPS_VP_ROOT)/common/include/uvmc-2.3.1 \
	-I$(SNPS_VP_ROOT)/common/include/tlm_serial \
	-I$(SNPS_VP_ROOT)/../IP/tlm_ft_can_bus/SystemC/include \
	-I$(SNPS_VP_ROOT)/../IP/tlm_ft_spi_bus/SystemC/include \
	-I$(SNPS_VP_ROOT)/../IP/tlm_ft_lin_bus/SystemC/include \
	-I$(SNPS_VP_ROOT)/common/include/scml2_protocol_engines/tlm2_ft_target_port/include \
	-I$(SNPS_VP_ROOT)/common/include/scml2_protocol_engines/tlm2_ft_initiator_port/include

BUILD_FLAGS := $(VP_BASE_INCLUDES) \
	-fmessage-length=0 -fno-var-tracking-assignments --param inline-unit-growth=300 \
	-Wno-deprecated -std=c++17 -O3 -fvisibility=hidden -DNDEBUG -fPIC \
	-DSC_INCLUDE_DYNAMIC_PROCESSES $(OCH_DEFINES) \
	-I$(OCH_SEP_ROOT)/SystemC/include -I$(OCH_SEP_ROOT) \
	$(SEP_INCLUDES) -ftemplate-depth=4096

TEST_BUILD_FLAGS := $(BUILD_FLAGS) \
	-I$(SNPS_VP_ROOT)/common/include/scml2_protocol_engines/tlm2_ft_target_port/include/testing \
	-I$(SNPS_VP_ROOT)/common/include/scml2_protocol_engines/tlm2_ft_initiator_port/include/testing \
	-I$(OCH_SEP_ROOT)/Tests/Unittests -I$(OCH_SEP_ROOT)/Tests

# Link static SEP models + boost/openssl into the wrapper .so (same as FastBuild USER_LD_FLAGS)
SEP_SHLIB_LIBS := -L$(SEP_LIB_DIR) -L$(BOOST_LIB) -L$(OPENSSL_LIB) -L$(CRYPTO_LIB) \
	-lOCHSEPModels -lboost_program_options -lboost_iostreams -lcrypto -lz -lstdc++fs -lssl

LINK_FLAGS := -shared -O3 -fPIC $(VP_RPATH) $(VP_RPATH_EXTRA) -Wl,--no-undefined \
	-L$(SNPS_VP_ROOT)/common/lib-gcc-9.5-64 \
	-L$(SNPS_VP_ROOT)/sd/coware/lib-gcc-9.5-64 \
	-L$(SNPS_VP_ROOT)/common/lib-any-64 \
	-uSnpsVPExtLinkHook -lSnpsVPExt -lSnpsVPFt -lSnpsVP -ltbb -Wl,--export-dynamic \
	-L$(SNPS_VP_ROOT)/common/libso-gcc-9.5-64 \
	-lomniORB4 -lomniDynamic4 -lomnithread -lpthread -ldwarf -lelf -lrt -ldl \
	$(SEP_SHLIB_LIBS)

# Test executable: SCML2 + VP runtime + SEP stack
LDFLAGS += $(VP_RPATH) $(VP_RPATH_EXTRA) \
	-L$(SNPS_VP_ROOT)/common/lib-gcc-9.5-64 \
	-Wl,--start-group \
	$(SEP_SHLIB_LIBS) \
	-lscml2_testing_systemc_main -lscml2_testing $(CWR_SCO2_LDFLAGS) \
	$(COVERAGE_FLAGS) $(EXTRA_LDFLAGS) -Wl,--end-group -m64

all: $(SEP_LIB) $(MODEL_LIB) $(TARGET)

# Built by CMake (riscv-vp-plusplus/vp): target merge_libochsep_models -> src/platform/sep/libOCHSEPModels.a
$(SEP_LIB):
	@test -f "$(SEP_LIB)" || { echo "Missing $(SEP_LIB) — build VP with -DVP_SYSC_BACKEND=synopsys (merges libaon_timer_model.a + libmailbox_model.a + … into libOCHSEPModels.a)"; exit 1; }

$(MODEL_LIB): $(OBJECTS) $(SEP_LIB)
	$(CXX_LINK) $(OBJECTS) -o $@ $(LINK_FLAGS)

$(TARGET): $(TEST_OBJECTS) $(MODEL_LIB)
	$(CXX_LINK) $(TEST_OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(BUILD_FLAGS) -MMD -MP -c $< -o $@

$(TEST_OBJECTS): $(TEST_SOURCES)
	$(CXX_LINK) $(TEST_BUILD_FLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

.PHONY: all clean sep-lib
sep-lib: $(SEP_LIB)

clean:
	rm -f $(OBJECTS) $(DEPS) $(TEST_OBJECTS) $(TEST_SOURCES:.cc=.d) $(MODEL_LIB) $(TARGET)

