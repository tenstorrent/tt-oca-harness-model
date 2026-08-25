// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <cstring>

constexpr size_t PAGE_SIZE = 4096;  // 4 KB

using Page = std::vector<uint8_t>;

inline size_t getPageBase(size_t address) {
    return address & ~(PAGE_SIZE - 1);
}

class PagedMemory {

  public:
    PagedMemory() = default;
    ~PagedMemory() = default;

    uint8_t read(size_t address) {
      size_t page_base = getPageBase(address);
      size_t offset = address % PAGE_SIZE;

      auto it = memory_map.find(page_base);
      if (it == memory_map.end()) {
        return 0;
      }

      Page& page = it->second;
      return page[offset];
    }

    void write(size_t address, uint8_t value) {
      size_t page_base = getPageBase(address);
      size_t offset = address % PAGE_SIZE;

      if (memory_map.find(page_base) == memory_map.end()) {
	      memory_map[page_base] = Page(PAGE_SIZE, 0);
      }

      Page& page = memory_map[page_base];
      page[offset] = value;
    }

    std::vector<uint8_t> readBytes(size_t address, size_t length) {
      std::vector<uint8_t> buffer(length);

      for (size_t i = 0; i < length; ++i) {
        buffer[i] = read(address + i);
      }

      return buffer;
    }

    void readBytes(size_t address, uint8_t* data, size_t length) {
      for (size_t i = 0; i < length; ++i) {
        data[i] = read(address + i);
      }
    }

    void writeBytes(size_t address, const uint8_t* data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            write(address + i, data[i]);
        }
    }

    void writeBytes(size_t address, const std::vector<uint8_t>& data) {
        writeBytes(address, data.data(), data.size());
    }

    // Get the number of allocated pages
    size_t getAllocatedPageCount() const {
        return memory_map.size();
    }

  uint8_t* getPtr(size_t address) {
    size_t page_base = getPageBase(address);
    size_t offset = address % PAGE_SIZE;

    auto it = memory_map.find(page_base);
    if (it == memory_map.end()) {
        return nullptr;       // memory not allocated
    }

    Page& page = it->second;
    return page.data() + offset;
  }



    // Clear all memory
    void clear() {
        memory_map.clear();
    }

  private:
    std::unordered_map<size_t, Page> memory_map;

};
