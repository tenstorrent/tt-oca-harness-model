# macOS port changes for tt-oca-harness-model VP (`sep-vp`)

This document records the changes made to build and link the SystemC virtual platform on **Apple Silicon macOS** (Darwin, Clang, libc++). The goal was to keep Linux (Ubuntu/RHEL) behavior working where possible and to fix macOS-specific toolchain, header, and linker issues without forking the codebase.

**Target:** `vp/build/bin/sep-vp`  
**Tested host:** macOS, Apple Clang, SystemC **3.0.1**, C++17 (`installs_c17` prefix)

---

## Quick build (macOS)

Set install paths once in your shell (example — adjust for your machine):

```bash
export SYSTEMC_HOME_C17=/path/to/installs_c17
export CCI_HOME_C17=/path/to/installs_c17
export OPENSSL_ROOT_C17=/path/to/installs_c17/openssl-3.0.13
export BOOST_ROOT_C17=/path/to/installs_c17/boost-1.84.0
export CMAKE_CXX_STANDARD=17

cd tt-oca-harness-model/vp
source ./configure_vp.sh          # zsh or bash — sets env only
./configure_vp.sh                 # configure + cmake (recommended)
cd build
make sep-vp -j$(sysctl -n hw.ncpu)
```

Use `*_C20` variables when `CMAKE_CXX_STANDARD=20`. See `vp/vp_build_env.sh` for the full convention.

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
| `vp/platform/sep/och_sep_ss.hpp` | SystemC 3.0 API | Yes — recommended for SystemC 3.0 everywhere |
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
| `sep/utils/csml/inc/csml_report.h` | Clang overload fix | Yes (submodule — commit separately) |
| `cmake/PeripheralCoverage.cmake` | Coverage linker flags (shared) | Yes — defers flags; `-lgcov` only on Linux |
| `sep/peripherals/run_all_peripherals.sh` | Batch test driver | Yes — uses `vp/vp_build_env.sh` |
| `sep/peripherals/*/CMakeLists.txt` (16 models) | Coverage + lcov targets | Yes — see [Coverage section](#peripheral-tests--coverage-macos) |
| `sep/peripherals/csrng/CMakeLists.txt` | csml ordering + coverage | Yes |
| `sep/peripherals/entropy_src/CMakeLists.txt` | Coverage target (find_program) | Yes |
| `sep/peripherals/key_manager/CMakeLists.txt` | Coverage linker flags | Yes |

**Untracked (new):** `cmake/PeripheralCoverage.cmake`, `sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC/` — add to git when the port is permanent.

---

## Per-file changes and rationale

### `cmake/FindSystemC.cmake`

**What changed:** When `SYSTEMC_HOME` is set, `find_package(SystemCLanguage …)` first searches `${SYSTEMC_HOME}/lib/cmake/SystemCLanguage` with `NO_DEFAULT_PATH` before falling back to generic hints.

**Why:** CMake cache can pin `SystemCLanguage_DIR` to a **different** install (e.g. C++20 build tree) while the project compiles as C++17. That produces link errors such as `sc_api_version_3_0_1_cxx201703L` vs `cxx202002L` — same SystemC version, different C++ standard ABI tags.

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
export SYSTEMC_HOME_C17=/opt/installs_c17
export CCI_HOME_C17=/opt/installs_c17
export OPENSSL_ROOT_C17=/opt/installs_c17/openssl-3.0.13
export BOOST_ROOT_C17=/opt/installs_c17/boost-1.84.0
export SYSTEMC_HOME_C20=/opt/installs_c20
export CCI_HOME_C20=/opt/installs_c20
export OPENSSL_ROOT_C20=/opt/installs_c20/openssl-3.0.13
export BOOST_ROOT_C20=/opt/installs_c20/boost-1.84.0
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

### `vp/platform/sep/och_sep_ss.hpp`

**What changed:**

```cpp
sc_core::sc_time(static_cast<double>(globalQuantumNs.get_param_value()),
                 sc_core::sc_time_unit::SC_NS);
```

**Why:** SystemC 3.0 adds deprecated `sc_time(T, bool)`. With a numeric first argument and `SC_NS`, Apple Clang reports ambiguous overload vs `sc_time(double, sc_time_unit)`. Explicit types pick the intended constructor.

**Linux impact:** Safe on all platforms; **recommended** when using SystemC 3.0 with GCC as well (GCC may silently pick one overload today; explicit is clearer).

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

### `sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC/` (new, untracked)

**What changed:** Copy of `RISCV-GCC` with `CC = clang` and `platform.h` defining `SOFTFLOAT_INTRINSIC_INT128` for macOS.

**Why:** Produce `softfloat.a` on Apple Silicon for linking into `veeriss_model`.

**Linux impact:** None — directory unused on Linux. Add to git if the port is permanent.

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

**Linux impact:** Safe on GCC. **Commit inside the `csml` submodule** and bump the submodule pointer in tt-oca-harness-model.

---

## Peripheral tests & coverage (macOS)

Standalone peripheral models are built and tested with `sep/peripherals/run_all_peripherals.sh` (Debug, ASAN, Coverage, CTest). On macOS, coverage required several fixes beyond the VP port above.

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
export SYSTEMC_HOME_C17=/path/to/installs_c17
export CCI_HOME_C17=/path/to/installs_c17
export OPENSSL_ROOT_C17=/path/to/installs_c17/openssl-3.0.13
export BOOST_ROOT_C17=/path/to/installs_c17/boost-1.84.0
export CMAKE_CXX_STANDARD=17
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

**Models updated** (16 files — same pattern in each):

`aes`, `aon_timer`, `csrng`, `edn`, `efuse`, `entropy_src`, `gpio`, `hmac`, `key_manager`, `kmac`, `lifecycle_ctrl`, `mailbox`, `otbn`, `secure_dma`, `spi_controller`, `spi_flash`, `uart_16550`

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

**What changed:** `add_subdirectory(…/csml)` moved from the top of the file to **after** `include(FindSystemC)`, guarded by `if(NOT TARGET csml_logger)`.

**Why:** `csml_logger` links `SystemC::systemc`. csrng was the only peripheral that added csml **before** SystemC discovery, so configure failed with `Target "csml_logger" links to: SystemC::systemc but the target was not found`. Other models (e.g. aes) already add csml after `FindSystemC`.

**Linux impact:** Safe — correct dependency order on all platforms.

---

### Coverage issues fixed on macOS (reference)

| Symptom | Root cause | Fix |
|---------|------------|-----|
| `Could NOT find Threads` (Coverage configure) | `--coverage`/`-lgcov` in `CMAKE_EXE_LINKER_FLAGS` before `find_package(Threads)` | `PeripheralCoverage.cmake` deferred linker flags |
| csrng Debug/ASAN/Coverage all fail | csml before `FindSystemC` | Reorder `add_subdirectory(csml)` in `csrng/CMakeLists.txt` |
| `lcov: No such file or directory` | Tool not installed | `brew install lcov`; script skips stage if missing |
| `SYSTEMC_HOME` empty in logs | Install paths not exported | Set `SYSTEMC_HOME_C17` (or `_C20`) per `vp/vp_build_env.sh` |
| lcov `inconsistent` / `format` / `mismatch` on SystemC headers | llvm-gcov + TLM/SystemC template code | `--ignore-errors …` on lcov/genhtml |
| genhtml `category` / `corrupt` errors | Same; register header templates | `--ignore-errors …` on genhtml |
| Coverage % always `n/a` | GNU `grep -oP` on macOS | `sed`-based `extract_coverage()` |
| Coverage shown as SKIP when lcov ran | `if ! $LCOV_AVAILABLE` when value is `true` | String compare `[ "$LCOV_AVAILABLE" != true ]`; skip return code `3` |

**Verified:** clean `./run_all_peripherals.sh --clean` passes Debug, ASAN, Coverage, and CTest for all 17 listed peripherals (e.g. aes 92.6%, aon_timer 98.1%, csrng 90.8%, edn 93.0%).

---

### Linux impact (coverage changes)

- **`PeripheralCoverage.cmake`:** Linux keeps `-lgcov`; deferred apply helps any platform where coverage linker flags break `FindThreads`.
- **`--ignore-errors` on lcov/genhtml:** Harmless on Linux/GCC; suppresses strict checks when they fire. Can stay for one codebase path.
- **`run_all_peripherals.sh`:** Uses `vp/vp_build_env.sh`; export `*_C17` / `*_C20` before running.

Re-validate on Linux after merge:

```bash
export SYSTEMC_HOME_C17=… CCI_HOME_C17=… BOOST_ROOT_C17=… OPENSSL_ROOT_C17=…
export CMAKE_CXX_STANDARD=17
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

1. **Install path env vars** — Each developer/CI job must export `SYSTEMC_HOME_C17`, `CCI_HOME_C17`, `OPENSSL_ROOT_C17`, `BOOST_ROOT_C17` (and `_C20` if needed). Documented in `vp/vp_build_env.sh`; nothing to edit in git per machine.

2. **SystemC 3.0 + C++ standard match** — Use C++17 SystemC with `CMAKE_CXX_STANDARD=17` (and vice versa for c20). `configure_vp.sh` pins `SystemCLanguage_DIR` to reduce mismatch; re-run configure after switching standard.

3. **Boost.Log removed from top-level find** — OK if Linux install also lacks `boost_log` or the component is unused. If some Linux image relied on Boost.Log, rebuild Boost or restore the component in cmake only where needed.

4. **`csml` submodule** — Ensure `csml_report.h` change is committed in submodule and referenced from parent repo.

### Recommended Linux re-validation

On a known-good Ubuntu/RHEL machine after merge:

```bash
export SYSTEMC_HOME_C17=…/install_c17
export CCI_HOME_C17=…/install_c17
export BOOST_ROOT_C17=…/boost-1.84.0
export OPENSSL_ROOT_C17=…/openssl-3.0.13
export CMAKE_CXX_STANDARD=17
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

### Recommended before sharing with Linux team

| Item | Priority | Suggestion |
|------|----------|------------|
| `sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC/` untracked | **Medium** | Add to git if macOS is a supported platform; document one-time `make` under that directory. |
| `csml` submodule | **Medium** | Commit `csml_report.h` in the submodule and bump the parent repo pointer (see **csml_report.h** subsection above). |

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
| Ambiguous `sc_time` constructor | SystemC 3.0 deprecated overloads | `och_sep_ss.hpp` |
| Wrong SystemC ABI at link | Cached `SystemCLanguage_DIR` | `FindSystemC.cmake`, `configure_vp.sh` |
| `vncserver` not found | Unused legacy link | `platform/infra/CMakeLists.txt` |
| `Could NOT find Threads` (Coverage) | Coverage linker flags before `FindThreads` | `cmake/PeripheralCoverage.cmake` |
| csrng `SystemC::systemc` not found | csml before `FindSystemC` | `csrng/CMakeLists.txt` |
| `lcov` / genhtml failures on macOS | llvm-gcov + SystemC headers | peripheral `CMakeLists.txt` `--ignore-errors` |
| `run_all_peripherals.sh` grep/nproc | BSD vs GNU tools | `run_all_peripherals.sh` |
| `ld: ignoring duplicate libraries` (`.a`) | Redundant entries on `sep-vp` link line | `vp/platform/sep/CMakeLists.txt` |

---

## Install layout reminder (macOS)

Single SystemC install uses versioned symlinks (not two versions):

```text
libsystemc.3.0.1.dylib   ← real library
libsystemc.3.0.dylib     → libsystemc.3.0.1.dylib
libsystemc.dylib         → libsystemc.3.0.dylib
```

Use **one** prefix per build (`installs_c17` for C++17, `installs_c20` for C++20) and match `CMAKE_CXX_STANDARD` to the SystemC build’s C++ standard.

---

## Related reading

- Main build docs: `README.md`
- VP configure: `vp/configure_vp.sh`, `vp/vp_build_env.sh`
- Peripheral batch tests: `sep/peripherals/run_all_peripherals.sh`
- Shared cmake modules: `cmake/FindSystemC.cmake`, `cmake/FindCCI.cmake`, `cmake/PeripheralCoverage.cmake`
