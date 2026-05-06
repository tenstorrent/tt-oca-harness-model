/*
 * sfdp_utils.cpp
 *
 *  Created on: Jan 21, 2026
 *      Author: ctr-sdangi
 */
#include "sfdp.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>

// ============================================================================
// SFDP UTILITY FUNCTIONS
// ============================================================================

// PUBLIC: Helper function to read a 32-bit DWORD from byte vector (little-endian)
uint32_t read_le32(const std::vector<uint8_t>& buf, size_t offset)
{
    if (offset + 4 > buf.size()) {
        return 0xFFFFFFFF;
    }

    return (static_cast<uint32_t>(buf[offset + 0]) << 0)  |
           (static_cast<uint32_t>(buf[offset + 1]) << 8)  |
           (static_cast<uint32_t>(buf[offset + 2]) << 16) |
           (static_cast<uint32_t>(buf[offset + 3]) << 24);
}

