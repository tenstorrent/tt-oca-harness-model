// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

struct initiator_if {
   public:
	virtual ~initiator_if() {}

	virtual std::string name() = 0;
	virtual void halt() = 0;
};