MAKEFLAGS += --no-print-directory

# Release build on by default
RELEASE_BUILD ?= ON
# Path to pre-built SystemC directory (uses SYSTEMC_HOME env variable if set)
PREBUILT_SYSTEMC_DIR ?= $(SYSTEMC_HOME)

# SystemC backend selection for VP build (matches vp/CMakeLists.txt cache variable).
VP_SYSC_BACKEND ?= accellera

# Build directory per backend to allow side-by-side builds.
# Accellera uses vp/build (default), Synopsys uses vp/build_synopsys.
ifeq ($(VP_SYSC_BACKEND),accellera)
VP_BUILD_DIR := vp/build
else
VP_BUILD_DIR := vp/build_$(VP_SYSC_BACKEND)
endif

ifeq ($(RELEASE_BUILD),ON)
	CMAKE_BUILD_TYPE = Release
else
	CMAKE_BUILD_TYPE = Debug
endif

# Default target: build sep-vp
.DEFAULT_GOAL := sep-vp

sep-vp: $(VP_BUILD_DIR)/Makefile
	$(MAKE) sep-vp -C $(VP_BUILD_DIR)

$(VP_BUILD_DIR)/Makefile:
	mkdir -p $(VP_BUILD_DIR)
	cmake -S vp -B $(VP_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-DVP_SYSC_BACKEND=$(VP_SYSC_BACKEND) \
		-DPREBUILT_SYSTEMC_DIR=$(PREBUILT_SYSTEMC_DIR)

clean:
	rm -rf vp/build vp/build_synopsys

.PHONY: sep-vp clean
