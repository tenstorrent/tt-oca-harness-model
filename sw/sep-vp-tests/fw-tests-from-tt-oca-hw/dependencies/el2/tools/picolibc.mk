# Stand-in for the VeeR EL2 picolibc makefile.
#
# common.mk calls this on every build.  picolibc is built once by
# dependencies/setup_dependencies.sh, so all this needs to do is fail loudly if
# that has not happened yet.
PICOLIBC_INSTALL := $(dir $(lastword $(MAKEFILE_LIST)))../third_party/picolibc/install
SETUP_SCRIPT     := $(dir $(lastword $(MAKEFILE_LIST)))../../setup_dependencies.sh

.PHONY: all
all:
	@test -f "$(PICOLIBC_INSTALL)/picolibc.specs" || { \
	  echo "picolibc is not built yet. Run: $(SETUP_SCRIPT)" >&2; \
	  exit 1; \
	}
