#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$repo_root"
rm -rf docs/och docs/och_source/_build
python3 -m sphinx -b html -d docs/och_source/_build/doctrees docs/och_source docs/och
touch docs/.nojekyll

echo "Built OCH documentation at ${repo_root}/docs/och/index.html"
