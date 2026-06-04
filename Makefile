MAKEFLAGS += --no-print-directory

# Release build on by default
RELEASE_BUILD ?= ON
# Path to pre-built SystemC directory (uses SYSTEMC_HOME env variable if set)
PREBUILT_SYSTEMC_DIR ?= $(SYSTEMC_HOME)

# SystemC backend selection for VP build (matches vp/CMakeLists.txt cache variable).

# Build directory per backend to allow side-by-side builds.
# Accellera uses vp/build (default)
VP_BUILD_DIR := vp/build

ifeq ($(RELEASE_BUILD),ON)
	CMAKE_BUILD_TYPE = Release
else
	CMAKE_BUILD_TYPE = Debug
endif

# C++ standard for CMake (17 or 20). Precedence: make CLI > environment >
# CXX_STD=c++NN (from configure_vp.sh) > default 17.
ifndef CMAKE_CXX_STANDARD
ifdef CXX_STD
CMAKE_CXX_STANDARD := $(patsubst c++%,%,$(CXX_STD))
else
CMAKE_CXX_STANDARD := 17
endif
endif

# Default target: build sep-vp
.DEFAULT_GOAL := sep-vp

sep-vp: submodule-init $(VP_BUILD_DIR)/Makefile
	$(MAKE) sep-vp -C $(VP_BUILD_DIR)

# Ensure git submodules (e.g. sep/utils/csml) are initialised and up to date.
# Safe to run repeatedly — no-ops when already in sync.
submodule-init:
	@git submodule update --init --recursive --quiet

$(VP_BUILD_DIR)/Makefile:
	mkdir -p $(VP_BUILD_DIR)
	cmake -S vp -B $(VP_BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
		-DCMAKE_CXX_STANDARD=$(CMAKE_CXX_STANDARD) \
		-DPREBUILT_SYSTEMC_DIR=$(PREBUILT_SYSTEMC_DIR)

# Repo root (always use absolute paths so clean works even if cwd is stale).
TENSTORRENT_SEP_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# From repo root: make clean
# From vp/build (after cmake configure): make clean-workdir  (or make clean for CMake only)
clean:
	@if [ -f "$(TENSTORRENT_SEP_ROOT)/$(VP_BUILD_DIR)/Makefile" ]; then \
		echo "Cleaning $(VP_BUILD_DIR)..."; \
		cmake --build "$(TENSTORRENT_SEP_ROOT)/$(VP_BUILD_DIR)" --target clean-workdir; \
	else \
		echo "Skipping VP clean: $(VP_BUILD_DIR)/Makefile not found."; \
		echo "  Configure first: make sep-vp   (or: cmake -S vp -B $(VP_BUILD_DIR))"; \
	fi

.PHONY: sep-vp submodule-init clean
