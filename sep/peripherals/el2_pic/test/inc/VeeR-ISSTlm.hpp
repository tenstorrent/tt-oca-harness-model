// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
// Test-only stand-in for the real cpu/VeeR-ISSTlm/model/inc/VeeR-ISSTlm.hpp.
//
// el2_pic.cpp only needs a hart type exposing trigger_external_interrupt(),
// clear_external_interrupt(), set_pic_claim_id(), peek_csr(), poke_csr() —
// exactly what MockHart already implements. Aliasing VeeRISSTlm to MockHart
// here lets the real el2_pic.cpp compile and run in the test build without
// pulling in the full ISS (Boost, Whisper's Hart.hpp/HartConfig.hpp/
// WhisperMessage.h) that the production VeeR-ISSTlm.hpp depends on.
//
// This header is found instead of the real one because el2_pic_testmodel_lib
// (test/inc) is on the include path and does not add the real
// cpu/VeeR-ISSTlm directories.
#include "mock_hart.h"

// el2_pic.h forward-declares `class VeeRISSTlm;` — a `using` alias can't
// coexist with that (an elaborated-type-specifier can't follow a typedef
// name), so this must be a real class, not an alias. It adds nothing to
// MockHart; it exists purely so the name/kind matches the forward decl.
class VeeRISSTlm : public MockHart {};
