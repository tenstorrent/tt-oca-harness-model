// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#include "sep_memory.h"

#include <algorithm>
#include <iomanip>
#include <sstream>


SEPMemory::SEPMemory(sc_module_name name, bool read_only)
: sc_module(name) 
, verbosity("verbosity", REG_DEFAULT_VERBOSITY)
, tsock("tsock")
, m_read_only(read_only) {

    logger.setMaxVerbosity(verbosity.get_param_value());
    logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
    logger.setFunctionTrace(false);

    tsock.register_b_transport(this, &SEPMemory::b_transport);
    tsock.register_get_direct_mem_ptr(this, &SEPMemory::get_direct_mem_ptr);
    tsock.register_transport_dbg(this, &SEPMemory::transport_dbg);
}

void SEPMemory::load_data(const char *src, uint64_t dst_addr, size_t n) {
  
  m_mem.writeBytes(dst_addr, reinterpret_cast<const uint8_t*>(src), n);
}

void SEPMemory::load_zero(uint64_t dst_addr, size_t n) {
  
  std::vector<uint8_t> zero_data(n, 0);
  m_mem.writeBytes(dst_addr, zero_data);
}

void SEPMemory::load_binary_file(const std::string &filename, uint64_t addr) {
    /*
     * check, if file exists, is readable and don't has zero size
     * (prevent segfault on mapped_source_file)
     */
      std::ifstream file;
      file.open(filename, std::ifstream::in | std::ifstream::binary | std::ios::ate);
      if (file.fail() || file.tellg() == 0) {
        REG_ERROR(0, logger) << "Open failed: \"" << filename << "\"" << std::endl;
        assert(0);
      }
      file.close();

      boost::iostreams::mapped_file_source mf(filename);
      assert(mf.is_open());
      m_mem.writeBytes(addr, reinterpret_cast<const uint8_t*>(mf.data()), mf.size());
}

void SEPMemory::b_transport(TRANS& trans, sc_core::sc_time& delay) { 
  tlm::tlm_command cmd = trans.get_command();
  uint64_t addr = trans.get_address();
  auto *ptr = trans.get_data_ptr();
  auto len = trans.get_data_length();

  if (ptr == nullptr && len != 0) {
    trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return;
  }

	// Guard the hex-dump string building behind the verbosity threshold. Without this,
	// the ostringstream construction and format loop run on EVERY transaction
	// (every fetch/load/store) even though REG_DEBUG(5) suppresses the actual emission at
	// the configured verbosity. These unnecessary loop passes make the whole sim crawl.
	if (logger.getMaxVerbosity() >= 5) {
		std::ostringstream oss;
		const auto dump_len = std::min<unsigned>(len, 16);
		for (unsigned i = 0; i < dump_len; ++i) {
			if (i) oss << " ";
			oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ptr[i]);
		}
		REG_DEBUG(5, logger) << name() << " "
			<< (cmd == tlm::TLM_WRITE_COMMAND ? "WRITE" : (cmd == tlm::TLM_READ_COMMAND ? "READ" : "OTHER"))
			<< " addr=0x" << std::hex << addr << " len=" << std::dec << len
			<< " data[0.." << (dump_len ? (dump_len - 1) : 0) << "]=" << oss.str() << std::endl;
	}

  if(cmd == tlm::TLM_WRITE_COMMAND) {
    if(!m_read_only) {
      const auto* be = trans.get_byte_enable_ptr();
      const unsigned be_len = trans.get_byte_enable_length();
      if (be != nullptr && be_len == 0) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
      }
      if (be == nullptr) {
        m_mem.writeBytes(addr, reinterpret_cast<const uint8_t*>(ptr), len);
      } else {
        for (unsigned i = 0; i < len; ++i) {
          if (be[i % be_len] == TLM_BYTE_ENABLED)
            m_mem.write(addr + i, ptr[i]);
        }
      }
    } else {
        // ROM semantics: accept the bus transaction but ignore writes.
        // This matches expectations from firmware tests (e.g. rom_sanity_test)
        // that attempt to write ROM and verify contents remain unchanged.
        (void)addr;
        (void)ptr;
        (void)len;
    }
  } else if(cmd == tlm::TLM_READ_COMMAND) {
      m_mem.readBytes(addr, ptr, len);
		if (logger.getMaxVerbosity() >= 5) {
			std::ostringstream oss;
			const auto dump_len = std::min<unsigned>(len, 16);
			for (unsigned i = 0; i < dump_len; ++i) {
				if (i) oss << " ";
				oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ptr[i]);
			}
			REG_DEBUG(5, logger) << name() << " READ-RSP"
				<< " addr=0x" << std::hex << addr << " len=" << std::dec << len
				<< " data[0.." << (dump_len ? (dump_len - 1) : 0) << "]=" << oss.str() << std::endl;
		}
  } else {
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      return;
  }

  delay += sc_core::sc_time(10, sc_core::SC_NS);
  trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

bool SEPMemory::get_direct_mem_ptr(TRANS& trans, tlm::tlm_dmi& dmi) {
  uint64_t addr = trans.get_address();
  
  uint64_t page_start = addr & ~(PAGE_SIZE - 1);
  uint64_t page_end = page_start + PAGE_SIZE - 1;

  // Get pointer to the start of the page
  uint8_t* page_ptr = m_mem.getPtr(page_start);
  if (!page_ptr) {
      return false;  // Page not allocated
  }

  // DMI pointer starts at the requested address within the page
  uint64_t offset = addr - page_start;
  dmi.set_dmi_ptr(page_ptr + offset);
  dmi.set_start_address(addr);
  dmi.set_end_address(page_end);
  
  if (m_read_only) {
      dmi.allow_read();
  } else {
      dmi.allow_read_write();
  }

  return true;
}

unsigned SEPMemory::transport_dbg(TRANS& trans) { 
  tlm::tlm_command cmd = trans.get_command();
  uint64_t addr = trans.get_address();
  auto *ptr = trans.get_data_ptr();
  auto len = trans.get_data_length();

  if (ptr == nullptr && len != 0) {
    trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    return 0;
  }

	{
		std::ostringstream oss;
		const auto dump_len = std::min<unsigned>(len, 16);
		for (unsigned i = 0; i < dump_len; ++i) {
			if (i) oss << " ";
			oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ptr[i]);
		}
		REG_DEBUG(5, logger) << name() << " DBG "
			<< (cmd == tlm::TLM_WRITE_COMMAND ? "WRITE" : (cmd == tlm::TLM_READ_COMMAND ? "READ" : "OTHER"))
			<< " addr=0x" << std::hex << addr << " len=" << std::dec << len
			<< " data[0.." << (dump_len ? (dump_len - 1) : 0) << "]=" << oss.str() << std::endl;
	}

  if(cmd == tlm::TLM_WRITE_COMMAND) {
    if (!m_read_only) {
      m_mem.writeBytes(addr, reinterpret_cast<const uint8_t*>(ptr), len);
    }
  } else if(cmd == tlm::TLM_READ_COMMAND) {
      m_mem.readBytes(addr, ptr, len);
		{
			std::ostringstream oss;
			const auto dump_len = std::min<unsigned>(len, 16);
			for (unsigned i = 0; i < dump_len; ++i) {
				if (i) oss << " ";
				oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ptr[i]);
			}
			REG_DEBUG(5, logger) << name() << " DBG READ-RSP"
				<< " addr=0x" << std::hex << addr << " len=" << std::dec << len
				<< " data[0.." << (dump_len ? (dump_len - 1) : 0) << "]=" << oss.str() << std::endl;
		}
  } else {
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      return 0;
  }

  trans.set_response_status(tlm::TLM_OK_RESPONSE);
  return len;
}
