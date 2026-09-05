# Contributing to tt-oca-harness-model

Thank you for your interest in contributing to the Open Chiplet Atlas
virtual platform.

## How to Contribute

### Reporting Bugs

Bugs are reported via [GitHub Issues](https://github.com/tenstorrent/tt-oca-harness-model/issues):

1. Search existing issues to avoid duplicates.
2. Provide a clear title and description.
3. Include steps to reproduce, expected versus actual behavior, and your
   environment (OS, compiler, SystemC/CCI versions).
4. Attach relevant logs. Do not attach secrets, credentials, or customer data.

Security vulnerabilities must **not** be filed as public issues. See
[SECURITY.md](SECURITY.md).

### Submitting Changes

Bug fixes and new functionality are submitted via
[Pull Requests](https://github.com/tenstorrent/tt-oca-harness-model/pulls):

1. Branch from `main`. Never commit directly to `main`.
2. Match the surrounding code style. New or edited hand-written register
   models must use `common/include/reg_access.h` (`regmodel`) rather than
   inline mask arithmetic. Runtime knobs go through Accellera CCI
   (`cci::cci_param`), not ad-hoc globals.
3. Add SPDX headers to new source files:

   ```c++
   // SPDX-License-Identifier: Apache-2.0
   // SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
   ```

4. Test what you touched. Peripheral changes need `./run_tests.sh`,
   `./run_tests.sh --asan`, and `./run_tests.sh --coverage` (line coverage
   on touched sources at least 95 percent). Do not combine ASan and coverage
   in one build.
5. Use a clear, imperative commit subject. Explain *why* in the body when
   the change is not obvious.

### Review Process

- Pull requests are reviewed on a **weekly** cadence.
- Maintainers may request changes. Once approved, the change is merged to
  `main`.

## Documentation

Architecture and register maps live in the hardware TRM (`tt-oca-hw`).
This repository documents SystemC/TLM-2.0 models, test plans, and how to
run tests. The customer-facing usage guide is
[`doc/SystemC_Virtual_Platform_Customer_Guide.md`](doc/SystemC_Virtual_Platform_Customer_Guide.md).

Documentation, images, and generated HTML are licensed under CC-BY 4.0
([LICENSE-DOCS](LICENSE-DOCS)). Software is Apache 2.0 ([LICENSE](LICENSE)).

## Code of Conduct

This project follows a [Code of Conduct](CODE_OF_CONDUCT.md). By
participating, you are expected to uphold it.

## License

By contributing, you agree that your contribution is licensed under the
Apache License, Version 2.0. See [LICENSE](LICENSE), [NOTICE](NOTICE), and
[LICENSE_understanding.txt](LICENSE_understanding.txt).
