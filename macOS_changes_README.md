# macOS port notes for tt-oca-harness-model

This document records the Apple Silicon (Darwin, Clang, libc++) port and the
current macOS build recipe. The port kept Linux (Ubuntu/RHEL) working and
fixed toolchain, header, and linker issues without forking the codebase.
Historical CSML / SystemC 3.0.1 / C++17-only notes are marked as such.

**Targets:** `sep-vp`, `smc-vp`, `smu-vp`  
**Current toolchain:** Apple Clang, SystemC **3.0.2**, CCI **1.0.2**, C++20
(default in `vp/configure_vp.sh`; required for `smc-vp` / `smu-vp`). C++17
SystemC still works for SEP-only builds if you set `CMAKE_CXX_STANDARD=17`
and point at a C++17 SystemC prefix.

Library pins (same as CI / `RELEASE_NOTES.md`): Boost ≥ 1.74 (examples use
1.84.0), OpenSSL 3.x (RHEL CI uses 3.3.2; a local `openssl-3` prefix is
fine). CCI 1.0.1, Boost.Regex, Boost.Log, and libvncserver are not required.

---

## Quick build (macOS)

Set install paths once in your shell (example — adjust for your machine).
`configure_vp.sh` defaults to C++20:

```bash
export SYSTEMC_HOME_C20=/path/to/installs_c20
export CCI_HOME_C20=/path/to/installs_c20
export OPENSSL_ROOT_C20=/path/to/installs_c20/openssl-3
export BOOST_ROOT_C20=/path/to/installs_c20/boost-1.84.0
# CMAKE_CXX_STANDARD defaults to 20; export 17 only for a SEP C++17 tree.

cd tt-oca-harness-model/vp
source ./configure_vp.sh          # zsh or bash — sets env only
./configure_vp.sh                 # configure + cmake (recommended)
cd build
make sep-vp -j$(sysctl -n hw.ncpu)
# smc-vp / smu-vp also need WHISPER_HOME and a C++20 SystemC/CCI prefix:
# make smc-vp smu-vp -j$(sysctl -n hw.ncpu)
```

Use `*_C17` variables when `CMAKE_CXX_STANDARD=17`. Unsuffixed `SYSTEMC_HOME`,
`CCI_HOME`, `OPENSSL_ROOT`, and `BOOST_ROOT` work as a fallback. See
`vp/vp_build_env.sh` for the full convention.

Re-run `./configure_vp.sh` after changing `CMAKE_CXX_STANDARD` or install prefixes so `SystemCLanguage_DIR` and RPATH stay aligned.

**Peripheral tests (optional):** `cd sep/peripherals && ./run_all_peripherals.sh --clean` — requires `brew install lcov` on macOS. See [Peripheral tests & coverage](#peripheral-tests--coverage-macos).

---

## Changed files (summary table)

| File | Category | Linux/RHEL safe? |
|------|----------|------------------|
| `cmake/FindSystemC.cmake` | CMake / SystemC discovery | Yes — improves all platforms |
| `vp/configure_vp.sh` | Env + cmake driver | Yes — no hardcoded paths; requires env vars |
| `vp/vp_build_env.sh` | Shared install-path resolution | Yes — C++17/20 via `*_C17` / `*_C20` env vars |
| `vp/CMakeLists.txt` | Top-level VP cmake | Yes |
| `vp/platform/sep/CMakeLists.txt` | `sep-vp` link line | Yes — better on all platforms |
| `vp/platform/infra/CMakeLists.txt` | Platform infra deps | Yes — removes unused Linux libs |
| `vp/platform/sep/sep_platform.hpp` / `src/sep_platform.cpp` | SystemC 3.0 / TLM quantum | Yes — `simtlm::install_global_quantum_ns_if_unset` |
| `sep/cpu/CMakeLists.txt` | VeeR ISS model | Yes |
| `sep/cpu/VeeR-ISS/float.cpp` | x86 SSE guard | Yes — no change on x86 Linux with `SOFT_FLOAT` |
| `sep/cpu/VeeR-ISS/vector.cpp` | Wide-int traits | Yes |
| `sep/cpu/VeeR-ISS/wideint.hpp` | Wide-int traits | Yes |
| `sep/cpu/VeeR-ISS/softfloat/CMakeLists.txt` | SoftFloat build dir | Yes — Linux still uses `RISCV-GCC` |
| `sep/peripherals/edn/CMakeLists.txt` | OpenSSL link | Yes — CMake best practice |
| `sep/peripherals/sep_memory/CMakeLists.txt` | Boost link | Yes |
| `sep/peripherals/spi_controller/include/spi_controller_register.h` | Register names | Yes |
| `sep/peripherals/spi_controller/include/spi_controller.h` | Header guard | Yes |
| `sep/peripherals/spi_controller/src/spi_controller.cpp` | Register field access | Yes |
| `sep/utils/csml/inc/csml_report.h` | Clang overload fix (**historical**) | CSML is not a live dependency — do not revive |
| `cmake/PeripheralCoverage.cmake` | Coverage linker flags (shared) | Yes — defers flags; `-lgcov` only on Linux |
| `sep/peripherals/run_all_peripherals.sh` | Batch test driver | Yes — uses `vp/vp_build_env.sh` |
| `sep/peripherals/*/CMakeLists.txt` | Coverage + lcov targets | Yes — see [Coverage section](#peripheral-tests--coverage-macos) |
| `sep/peripherals/csrng/CMakeLists.txt` | csml ordering + coverage | Yes |
| `sep/peripherals/entropy_src/CMakeLists.txt` | Coverage target (find_program) | Yes |
| `sep/peripherals/key_manager/CMakeLists.txt` | Coverage linker flags | Yes |

**Now tracked:** `cmake/PeripheralCoverage.cmake` and `sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC/` are in the tree.

---

## Per-file changes and rationale

### `cmake/FindSystemC.cmake`

**What changed:** When `SYSTEMC_HOME` is set, `find_package(SystemCLanguage …)` first searches `${SYSTEMC_HOME}/lib/cmake/SystemCLanguage` with `NO_DEFAULT_PATH` before falling back to generic hints.

**Why:** CMake cache can pin `SystemCLanguage_DIR` to a **different** install (e.g. a C++17 tree while the project compiles as C++20). That produces link errors such as `sc_api_version_3_0_2_cxx201703L` vs `cxx202002L` — same SystemC version, different C++ standard ABI tags.

**Linux impact:** Safe and beneficial. Prevents stale/wrong SystemC package discovery on multi-prefix machines. Pair with `configure_vp.sh` passing `-DSystemCLanguage_DIR=…`.

---

### `vp/configure_vp.sh` + `vp/vp_build_env.sh`

**What changed:**

1. **zsh compatibility** — resolve script path via `${(%):-%N}`; detect sourced vs executed without applying `set -e` to the parent shell when sourced.
2. **No hardcoded install paths** — `configure_vp.sh` sets only `CMAKE_BUILD_TYPE` and `CMAKE_CXX_STANDARD` defaults. All SystemC/OpenSSL/Boost/CCI prefixes come from the environment.
3. **C++17 vs C++20** — `vp_build_env.sh` selects `SYSTEMC_HOME_C17`, `CCI_HOME_C17`, … or `*_C20` based on `CMAKE_CXX_STANDARD`. Unsuffixed `SYSTEMC_HOME`, `CCI_HOME`, … work as a fallback when you use one prefix for both standards.
4. **Host lib layout** — `vp_prefix_libdir()` / `vp_openssl_libdir()` pick `lib/` vs `lib-linux64`/`lib64` by OS (Ubuntu, RHEL, macOS); not machine-specific paths.
5. **Runtime loader** — `DYLD_LIBRARY_PATH` on Darwin, `LD_LIBRARY_PATH` on Linux.
6. **CMake pins** — `-DSystemCLanguage_DIR`, `-DSystemCCCI_DIR`, RPATH for OpenSSL/Boost/SystemC/CCI.
7. **`CMAKE_EXTRA` guard** — only expand empty array when non-empty (avoids `set -u` failure).

**Why:** Avoids macOS-only paths in git that block Linux merges. Each developer sets paths for their machine in shell profile or CI env.

**Example (`~/.bashrc` / `~/.zshrc`):**

```bash
export SYSTEMC_HOME_C20=/opt/installs_c20
export CCI_HOME_C20=/opt/installs_c20
export OPENSSL_ROOT_C20=/opt/installs_c20/openssl-3
export BOOST_ROOT_C20=/opt/installs_c20/boost-1.84.0
# Optional SEP-only C++17 prefix:
export SYSTEMC_HOME_C17=/opt/installs_c17
export CCI_HOME_C17=/opt/installs_c17
export OPENSSL_ROOT_C17=/opt/installs_c17/openssl-3
export BOOST_ROOT_C17=/opt/installs_c17/boost-1.84.0
```

**Linux impact:** Same workflow on Ubuntu and RHEL — export `*_C17` / `*_C20` for your install layout, then `source configure_vp.sh`.

---

### `vp/CMakeLists.txt`

**What changed:**

1. Removed `log` from `find_package(Boost …)` — installed Boost on macOS had `iostreams`, `program_options`, `system` only; nothing in VP/SEP uses Boost.Log.
2. `clean-workdir` also cleans `softfloat/build/Darwin-GCC` in addition to `RISCV-GCC`.

**Why:** Configure failed on missing `boost_log`. Darwin softfloat artifacts need cleaning on macOS.

**Linux impact:** Safe if Boost.Log is not required (verified: no `#include <boost/log/…>` in VP/SEP). Linux clean still runs `RISCV-GCC`.

---

### `vp/platform/sep/CMakeLists.txt`

**What changed:**

1. Replaced raw `-lboost_*`, `-lpthread`, `-ldl`, `${Boost_LIBRARIES}` with CMake targets: `Boost::iostreams`, `Boost::program_options`, `SystemC::systemc`, `Threads::Threads`, `z`.
2. Removed redundant direct links to `softfloat`, `csml_logger`, and `spi_flash_core` — they are already pulled in via `veeriss_model` and peripheral models (`PUBLIC` / `PRIVATE` deps). This eliminates `ld: warning: ignoring duplicate libraries` on macOS.

**Why:** Raw `-lboost_iostreams` has no `-L` path to the custom Boost prefix on macOS → `library 'boost_iostreams' not found`. Imported targets propagate include dirs and library paths. Explicit `.a` entries duplicated what model targets already export.

**Linux impact:** Safe — standard modern CMake; cleaner link line on all platforms.

---

### `vp/platform/infra/CMakeLists.txt`

**What changed:** `target_link_libraries(platform-infra INTERFACE SystemC::systemc)` instead of `systemc vncserver util`.

**Why:** `vncserver` and `util` are not referenced anywhere in VP sources; link failed on macOS (`library 'vncserver' not found`). They were legacy Linux package names.

**Linux impact:** Safe unless an undocumented downstream binary expected those libs at link time (none found in-tree). If VNC is needed later, gate with `find_library` and an option.

---

### `vp/platform/sep/sep_platform.hpp` / `src/sep_platform.cpp`

The SEP top module is still named `och_sep_ss`; the sources are
`sep_platform.hpp` and `sep_platform.cpp` (there is no `och_sep_ss.hpp`).

**What changed (port):** Apple Clang rejected the SystemC 3.0 `sc_time(T, bool)`
overload as ambiguous. The platform now installs the TLM global quantum via
`simtlm::install_global_quantum_ns_if_unset` in
[`common/include/tlm_quantum_policy.h`](common/include/tlm_quantum_policy.h)
instead of constructing `sc_time` inline:

```cpp
simtlm::install_global_quantum_ns_if_unset(globalQuantumNs.get_param_value());
```

**Linux impact:** Same helper on all platforms. Safe with SystemC 3.0.2.

---

### `sep/cpu/CMakeLists.txt`

**What changed:**

1. `find_package(Boost REQUIRED COMPONENTS program_options)` + link `Boost::headers`, `Boost::program_options`.
2. `stdc++fs` linked **only** for `GNU` — macOS libc++ does not use `libstdc++fs`.
3. `-mtune=native` applied **only** for `GNU` — ignored on Apple Clang; avoids tuning Release builds to one CPU generation when using GCC.

**Why:** `boost/format.hpp` and related headers needed for VeeR-ISS; macOS link failed without Boost include propagation. Unconditional `stdc++fs` breaks Apple Clang links. `-mtune=native` is a GCC tuning flag with no effect on the macOS port.

**Linux impact:** Safe. GCC Linux builds still get `stdc++fs` and `-mtune=native`; Clang/macOS builds skip both.

---

### `sep/cpu/VeeR-ISS/float.cpp`

**What changed:**

1. `#include <emmintrin.h>` only when **not** `SOFT_FLOAT` and on x86/x64.
2. `clearSimulatorFpFlags()` uses SSE on x86; `std::feclearexcept` elsewhere.

**Why:** VP builds with `SOFT_FLOAT`; unconditional x86 header fails on ARM64 macOS. SSE path is for native host FP (non–soft-float builds on x86 only).

**Linux impact:** No change for current VP (`SOFT_FLOAT` + ARM64/x86_64 Linux with soft float). x86 Linux native-FP builds unchanged.

---

### `sep/cpu/VeeR-ISS/vector.cpp` + `wideint.hpp`

**What changed:**

1. Removed illegal `namespace std { template<> struct make_signed/make_unsigned<WdRiscv::…> }` specializations from `vector.cpp`.
2. Added `WdRiscv::make_signed` / `make_unsigned` in `wideint.hpp` (delegate to `std::` for builtins; specialize wide types).
3. `vector.cpp` uses `make_signed` / `make_unsigned` (via `using namespace WdRiscv`).

**Why:** libc++ (Apple Clang) marks standard traits as non-specializable (`-Winvalid-specialization`). Linux libstdc++ often allowed this; it was undefined behavior.

**Linux impact:** Safe and **cleaner** on GCC/libstdc++ too — avoids UB; behavior unchanged for RVV wide types.

---

### `sep/cpu/VeeR-ISS/softfloat/CMakeLists.txt`

**What changed:** `if(APPLE)` → build `build/Darwin-GCC`; else `build/RISCV-GCC`.

**Why:** SoftFloat ships per-platform make trees; Linux makefile uses GCC flags unsuitable for Darwin/Clang.

**Linux impact:** Unchanged — non-Apple hosts still use `RISCV-GCC`.

---

### `sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC/` (tracked)

**What changed:** Copy of `RISCV-GCC` with `CC = clang` and `platform.h` defining `SOFTFLOAT_INTRINSIC_INT128` for macOS.

**Why:** Produce `softfloat.a` on Apple Silicon for linking into `veeriss_model`.

**Linux impact:** None — directory unused on Linux.

---

### `sep/peripherals/edn/CMakeLists.txt`

**What changed:**

1. `find_package(OpenSSL REQUIRED)`; link `OpenSSL::Crypto` instead of raw `-lcrypto`.
2. Coverage build: `PeripheralCoverage` + `lcov`/`genhtml --ignore-errors …` (same as other peripherals — see [Coverage section](#peripheral-tests--coverage-macos)).

**Why:** Raw `crypto` does not propagate include paths → `openssl/rand.h` not found during compile. Coverage on macOS needs shared linker and lcov fixes.

**Linux impact:** Safe — same pattern as aes/csrng/kmac; improves all platforms.

---

### `sep/peripherals/sep_memory/CMakeLists.txt`

**What changed:** `find_package(Boost REQUIRED COMPONENTS iostreams)`; link `Boost::iostreams`.

**Why:** `sep_memory.h` includes `boost/iostreams/device/mapped_file.hpp`; target had no Boost include path.

**Linux impact:** Safe.

---

### `sep/peripherals/spi_controller/` (register header, `.h`, `.cpp`)

**What changed:**

1. C++ field names `UNDERFLOW` / `OVERFLOW` → `underflow` / `overflow` (RDL/register path strings unchanged: `".UNDERFLOW"`, `".OVERFLOW"`).
2. `#pragma once` on `spi_controller.h`.
3. `.cpp` uses `ERROR_STATUS.underflow` / `.overflow`.

**Why:** macOS `<math.h>` defines `#define OVERFLOW 3` and `#define UNDERFLOW 4`. Those macros broke register class definitions and caused cascading SystemC errors (`SC_THREAD`, `wait()`, etc.).

**Linux impact:** Safe — glibc may not define those macros in all include orders; lowercase names are portable. Register bit semantics unchanged.

---

### `sep/utils/csml/inc/csml_report.h` (submodule)

**What changed:** Added non-template overloads for `(const char*, const char*)` and `(const string&, const char*)`; removed ambiguous two-argument template that clashed with the variadic overload on Clang.

**Why:** `CSML_REPORT(…, regname, " message")` failed with “call to 'form_report_string' is ambiguous” on Apple Clang.

**Linux impact:** Historical only. CSML is not in the tree (`sep/utils/` is `paged-memory/` and `tlm_extensions/`). SEP register models use in-house `regmodel` under `common/include`. Do not re-add `sep/utils/csml`.

---

## Peripheral tests & coverage (macOS)

Standalone peripheral models are built and tested with `sep/peripherals/run_all_peripherals.sh` (Release, ASAN, Coverage, CTest; coverage gate ≥ 95% line). On macOS, coverage required several fixes beyond the VP port above.

### Quick run

```bash
# Prerequisites: lcov on PATH (macOS: brew install lcov)
cd tt-oca-harness-model/sep/peripherals
./run_all_peripherals.sh              # incremental, all peripherals
./run_all_peripherals.sh --clean      # clean build dirs first
./run_all_peripherals.sh --clean aes csrng   # subset only
```

Logs: `sep/peripherals/logs/<peripheral>/` and top-level `logs/Full_result.log`.

Override install paths via the same `*_C17` / `*_C20` env vars as `configure_vp.sh` (see `vp/vp_build_env.sh`):

```bash
export SYSTEMC_HOME_C20=/path/to/installs_c20
export CCI_HOME_C20=/path/to/installs_c20
export OPENSSL_ROOT_C20=/path/to/installs_c20/openssl-3
export BOOST_ROOT_C20=/path/to/installs_c20/boost-1.84.0
# CMAKE_CXX_STANDARD defaults to 20 via configure_vp.sh
./run_all_peripherals.sh --clean
```

---

### `cmake/PeripheralCoverage.cmake` (new)

Shared module included from each peripheral `CMakeLists.txt` when `CMAKE_BUILD_TYPE=Coverage`.

**What it does:**

1. **`peripheral_coverage_link_flags()`** — appends `--coverage` to `CMAKE_EXE_LINKER_FLAGS` via `cmake_language(DEFER …)` so `find_package(Threads)` runs **before** linker flags are set (avoids `Could NOT find Threads` during configure).
2. **macOS:** linker flag is `--coverage` only (no `-lgcov` — Apple Clang/llvm-gcov has no `libgcov`).
3. **Linux:** `--coverage -lgcov` (unchanged from prior behavior).

**Usage in each peripheral** (inside the `Coverage` build-type block, after compile flags):

```cmake
include(PeripheralCoverage)
peripheral_coverage_link_flags()
```

Requires `list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")` (already present in model CMakeLists).

---

### `sep/peripherals/run_all_peripherals.sh`

**What changed:**

| Change | Why |
|--------|-----|
| Sources `vp/vp_build_env.sh` — same `*_C17` / `*_C20` env vars as VP | No machine-specific defaults; works on Ubuntu, RHEL, and macOS when paths are exported |
| `CMAKE_EXTRA_ARGS` passed to every `cmake` invocation | Pins `-DSystemCLanguage_DIR`, `-DSystemCCCI_DIR`, Boost/OpenSSL roots |
| `DYLD_LIBRARY_PATH` / `LD_LIBRARY_PATH` for runtime libs | Test binaries find SystemC/CCI/OpenSSL at runtime |
| `nproc` → `sysctl -n hw.ncpu` (macOS) / `getconf` fallback | `nproc` missing on macOS |
| `extract_coverage()` uses `sed` instead of `grep -oP` | BSD/macOS `grep` has no Perl regex |
| `LCOV_AVAILABLE` check; return `3` when `lcov` missing | Coverage reported as **SKIP**, not **FAIL**, when tool absent |
| `[ "$LCOV_AVAILABLE" != true ]` for skip test | Avoid `if ! $LCOV_AVAILABLE` (expands to `if ! true`, inverted bash logic) |

**Prerequisite:** `lcov` and `genhtml` on `PATH`. On macOS: `brew install lcov`.

---

### Per-peripheral `CMakeLists.txt` (coverage targets)

**Models that use this coverage pattern** (current tree; `gpio` / `uart_16550` are not in git):

`adams_bridge`, `aes`, `aon_timer`, `csrng`, `edn`, `efuse`, `el2_pic`, `entropy_src`, `hmac`, `key_manager`, `kmac`, `lifecycle_ctrl`, `local_master_alias_remap_ctrl`, `mailbox`, `otbn`, `secure_dma`, `sep_cpu_ctrl`, `sep_filter_ctrl`, `sep_output_remap_ctrl`, `sep_reset_ctrl`, `sep_scratch_cold`, `sep_scratch_warm`, `spi_controller`, `spi_flash`

**Typical changes:**

1. **Linker flags** — replace inline  
   `set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")`  
   with `include(PeripheralCoverage)` + `peripheral_coverage_link_flags()`.

2. **`lcov` / `genhtml` error handling** — Apple Clang’s gcov (llvm-gcov) and SystemC/TLM headers trigger lcov/genhtml errors that GCC/Linux often does not. All coverage targets now pass:

   ```text
   lcov:     --ignore-errors inconsistent,unsupported,format,mismatch
   genhtml:  --ignore-errors inconsistent,unsupported,format,corrupt,category
   ```

   on `--capture`, `--extract`, and HTML generation. Reports still filter to each model’s `src/*` and `include/*`; third-party headers are excluded from the filtered report but may appear during capture.

3. **`entropy_src`** — uses `find_program(LCOV_PATH lcov)` / `GENHTML_PATH`; same `--ignore-errors` flags applied to `${LCOV_PATH}` / `${GENHTML_PATH}` commands.

4. **`key_manager`** — added `PeripheralCoverage` (was missing linker flags in Coverage builds).

**Example** (`aes/CMakeLists.txt` coverage target):

```cmake
COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --capture ...
COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --extract ...
COMMAND genhtml --ignore-errors inconsistent,unsupported,format,corrupt,category ...
```

---

### `sep/peripherals/csrng/CMakeLists.txt` (configure order)

**What changed (historical):** `add_subdirectory(…/csml)` was moved after `include(FindSystemC)`.

**Why then:** `csml_logger` linked `SystemC::systemc` and csrng was the only peripheral that added CSML before SystemC discovery.

**Now:** CSML is gone. `csrng/CMakeLists.txt` uses in-house `regmodel` and does not add a csml subdirectory.

---

### Coverage issues fixed on macOS (reference)

| Symptom | Root cause | Fix |
|---------|------------|-----|
| `Could NOT find Threads` (Coverage configure) | `--coverage`/`-lgcov` in `CMAKE_EXE_LINKER_FLAGS` before `find_package(Threads)` | `PeripheralCoverage.cmake` deferred linker flags |
| csrng Debug/ASAN/Coverage all fail | csml before `FindSystemC` (historical) | CSML removed; `regmodel` only |
| `lcov: No such file or directory` | Tool not installed | `brew install lcov`; script skips stage if missing |
| `SYSTEMC_HOME` empty in logs | Install paths not exported | Set `SYSTEMC_HOME_C17` (or `_C20`) per `vp/vp_build_env.sh` |
| lcov `inconsistent` / `format` / `mismatch` on SystemC headers | llvm-gcov + TLM/SystemC template code | `--ignore-errors …` on lcov/genhtml |
| genhtml `category` / `corrupt` errors | Same; register header templates | `--ignore-errors …` on genhtml |
| Coverage % always `n/a` | GNU `grep -oP` on macOS | `sed`-based `extract_coverage()` |
| Coverage shown as SKIP when lcov ran | `if ! $LCOV_AVAILABLE` when value is `true` | String compare `[ "$LCOV_AVAILABLE" != true ]`; skip return code `3` |

**Verified (port era):** clean `./run_all_peripherals.sh --clean` passed Debug/ASAN/Coverage/CTest on macOS for the peripherals then in tree. The coverage **gate is now ≥ 95% line**; re-run the orchestrator for current percentages. Do not treat the August 2026 sample figures as current.

---

### Linux impact (coverage changes)

- **`PeripheralCoverage.cmake`:** Linux keeps `-lgcov`; deferred apply helps any platform where coverage linker flags break `FindThreads`.
- **`--ignore-errors` on lcov/genhtml:** Harmless on Linux/GCC; suppresses strict checks when they fire. Can stay for one codebase path.
- **`run_all_peripherals.sh`:** Uses `vp/vp_build_env.sh`; export `*_C17` / `*_C20` before running.

Re-validate on Linux after merge:

```bash
export SYSTEMC_HOME_C20=… CCI_HOME_C20=… BOOST_ROOT_C20=… OPENSSL_ROOT_C20=…
export CMAKE_CXX_STANDARD=20
cd sep/peripherals && ./run_all_peripherals.sh --clean aes
```

---

## Linux / RHEL compatibility assessment

### Expected to still work (code changes)

All **source and CMake link fixes** above are either portable or guarded by `APPLE` / `GNU` / `SOFT_FLOAT` / x86 checks. None intentionally break the prior Ubuntu/RHEL success path except where noted below.

Improvements that also help Linux:

- `OpenSSL::Crypto`, `Boost::*` imported targets
- `FindSystemC` pinned to `SYSTEMC_HOME`
- `WdRiscv::make_signed` / `make_unsigned` instead of illegal `std` specializations
- Removal of unused `vncserver` / `util` links

### Requires attention before sharing branch with Linux team

1. **Install path env vars** — Each developer/CI job must export `SYSTEMC_HOME_C20`, `CCI_HOME_C20`, `OPENSSL_ROOT_C20`, `BOOST_ROOT_C20` (and `_C17` if building SEP as C++17). Documented in `vp/vp_build_env.sh`; nothing to edit in git per machine.

2. **SystemC 3.0.2 + C++ standard match** — Default is C++20. Use a C++17 SystemC prefix only with `CMAKE_CXX_STANDARD=17` (SEP). `configure_vp.sh` pins `SystemCLanguage_DIR` to reduce mismatch; re-run configure after switching standard.

3. **Boost.Log removed from top-level find** — OK if Linux install also lacks `boost_log` or the component is unused. If some Linux image relied on Boost.Log, rebuild Boost or restore the component in cmake only where needed.

4. **`csml` (historical)** — No longer a live dependency. Do not commit into `sep/utils/csml`; SEP uses in-house `regmodel` under `common/include`.

### Recommended Linux re-validation

On a known-good Ubuntu/RHEL machine after merge:

```bash
export SYSTEMC_HOME_C20=…/install_c20
export CCI_HOME_C20=…/install_c20
export BOOST_ROOT_C20=…/boost-1.84.0
export OPENSSL_ROOT_C20=…/openssl-3
export CMAKE_CXX_STANDARD=20
cd vp && ./configure_vp.sh && cd build && make sep-vp -j$(nproc)
```

---

## Cleanup and simplification opportunities

Items below are **follow-ups**, not requirements for the macOS port. Builds and peripheral tests already pass without them.

### Done in this branch

| Item | Resolution |
|------|------------|
| Duplicate static `.a` on `sep-vp` link line | Removed `softfloat`, `csml_logger`, and `spi_flash_core` from `target_link_libraries(sep-vp …)` in `vp/platform/sep/CMakeLists.txt`. Each archive is linked once via `veeriss_model` or a peripheral model. |
| `softfloat/CMakeLists.txt` include path typo | Fixed `${SOURCE_DIR}//source/` → `${SOURCE_DIR}/source/`. |
| `-mtune=native` on `veeriss_model` | Applied only when `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"` in `sep/cpu/CMakeLists.txt`. |
| Install paths in `configure_vp.sh` | Removed hardcoded paths; `vp/vp_build_env.sh` resolves `*_C17` / `*_C20` from env. |

### Follow-ups (status)

| Item | Priority | Suggestion |
|------|----------|------------|
| `sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC/` | **Done** | Tracked in git. |
| `csml` (historical) | — | Not a live dependency. `sep/utils/csml` is not in the tree. |

### Optional polish (low priority)

| Item | Notes |
|------|--------|
| Float16 helpers in `vector.cpp` still in `namespace std` | Could move to `WdRiscv` for consistency; unrelated to macOS. |

### Not needed (verified on macOS)

| Item | Why closed |
|------|------------|
| `spi_controller.h` `get_clk_period()` | Uses `csml_param<double>` → `sc_time(double, SC_NS)` is already unambiguous. `spi_controller` passes Debug/ASAN/Coverage/CTest without change. |
| Duplicate `.a` trim (original backlog item) | Addressed — see **Done** above. |
| Centralize OpenSSL on `sep-vp` link line | Repeated `libcrypto` / `libssl` in `ld` output is cosmetic only. Each crypto peripheral must keep `OpenSSL::Crypto` / `OpenSSL::SSL` in its own `CMakeLists.txt` for **standalone** builds. Trimming at `sep-vp` only would not remove all duplicates anyway. |

No open high-priority cleanup items remain for cross-platform merges.

---

## macOS-specific issues addressed (reference)

| Symptom | Root cause | Fix location |
|---------|------------|--------------|
| `BASH_SOURCE[0]: parameter not set` | Sourced in zsh | `configure_vp.sh` |
| Shell exits after failed command | `set -e` when sourced | `configure_vp.sh` |
| `boost_log` not found | Unused component required | `vp/CMakeLists.txt` |
| `boost_iostreams` not found at link | Raw `-l` without `-L` | `platform/sep/CMakeLists.txt`, model CMakeLists |
| `emmintrin.h` on ARM64 | Unconditional x86 include | `float.cpp` |
| `make_signed` specialization error | libc++ forbids `std` trait spec | `wideint.hpp`, `vector.cpp` |
| `openssl/rand.h` not found (edn) | Raw `-lcrypto` | `edn/CMakeLists.txt` |
| SPI `SC_THREAD` / `wait()` errors | `OVERFLOW`/`UNDERFLOW` macros | `spi_controller_register.h` |
| Ambiguous `sc_time` constructor | SystemC 3.0 deprecated overloads | `sep_platform.cpp` + `tlm_quantum_policy.h` |
| Wrong SystemC ABI at link | Cached `SystemCLanguage_DIR` | `FindSystemC.cmake`, `configure_vp.sh` |
| `vncserver` not found | Unused legacy link | `platform/infra/CMakeLists.txt` |
| `Could NOT find Threads` (Coverage) | Coverage linker flags before `FindThreads` | `cmake/PeripheralCoverage.cmake` |
| csrng `SystemC::systemc` not found | csml before `FindSystemC` (historical) | CSML removed |
| `lcov` / genhtml failures on macOS | llvm-gcov + SystemC headers | peripheral `CMakeLists.txt` `--ignore-errors` |
| `run_all_peripherals.sh` grep/nproc | BSD vs GNU tools | `run_all_peripherals.sh` |
| `ld: ignoring duplicate libraries` (`.a`) | Redundant entries on `sep-vp` link line | `vp/platform/sep/CMakeLists.txt` |

---

## Install layout reminder (macOS)

Single SystemC install uses versioned symlinks (not two versions):

```text
libsystemc.3.0.2.dylib   ← real library
libsystemc.3.0.dylib     → libsystemc.3.0.2.dylib
libsystemc.dylib         → libsystemc.3.0.dylib
```

Use **one** prefix per build (`installs_c20` for the default C++20 tree, `installs_c17` only for SEP C++17) and match `CMAKE_CXX_STANDARD` to the SystemC build’s C++ standard.

---

## Related reading

- Main build docs: `README.md`
- Release / coverage / compiler matrix: `RELEASE_NOTES.md`
- VP configure: `vp/configure_vp.sh`, `vp/vp_build_env.sh`
- Peripheral batch tests: `sep/peripherals/run_all_peripherals.sh`
- SEP CPU tests: `sep/cpu/run_tests.sh`
- Shared cmake modules: `cmake/FindSystemC.cmake`, `cmake/FindCCI.cmake`, `cmake/PeripheralCoverage.cmake`
