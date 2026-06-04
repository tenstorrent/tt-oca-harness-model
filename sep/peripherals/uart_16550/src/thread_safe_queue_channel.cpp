#include "thread_safe_queue_channel.h"

thread_safe_queue_channel::thread_safe_queue_channel(const char* name)
    : sc_prim_channel(name), m_data_event((std::string(name) + "_data_event").c_str()) {
}

void thread_safe_queue_channel::write(uint8_t data) {
    // This method can be called from any thread (external to SystemC)
    // We add data to the pending queue and request an update
    m_pending_queue.push(data);
    
    // async_request_update() is thread-safe and schedules update() to be
    // called in the next delta cycle by the SystemC kernel
    async_request_update();
}

bool thread_safe_queue_channel::has_data() const {
    // This should only be called from SystemC threads
    return !m_queue.empty();
}

uint8_t thread_safe_queue_channel::read() {
    // This should only be called from SystemC threads when has_data() is true
    if (m_queue.empty()) {
        SC_REPORT_ERROR("thread_safe_queue_channel", "Attempted to read from empty queue");
        return 0;
    }
    
    uint8_t data = m_queue.front();
    m_queue.pop();
    return data;
}

const sc_core::sc_event& thread_safe_queue_channel::data_available_event() const {
    return m_data_event;
}

void thread_safe_queue_channel::update() {
    // This is called by the SystemC kernel in the update phase
    // Move all pending data to the main queue
    while (!m_pending_queue.empty()) {
        m_queue.push(m_pending_queue.front());
        m_pending_queue.pop();
    }
    
    // Notify that data is available
    if (!m_queue.empty()) {
        m_data_event.notify(SC_ZERO_TIME);
    }
}
