#pragma once

#include <systemc.h>
#include <queue>
#include <cstdint>

/**
 * @brief Interface for thread-safe queue operations
 * 
 * Defines the interface for writing data from external threads
 * and reading data from SystemC threads.
 */
class thread_safe_queue_if : public sc_core::sc_interface {
public:
    virtual ~thread_safe_queue_if() = default;
    
    /**
     * @brief Write data to the queue (thread-safe, can be called from any thread)
     * @param data Byte to write to the queue
     */
    virtual void write(uint8_t data) = 0;
    
    /**
     * @brief Check if queue has data (should be called from SystemC thread)
     * @return true if queue is not empty
     */
    virtual bool has_data() const = 0;
    
    /**
     * @brief Read data from the queue (should be called from SystemC thread)
     * @return Byte from the front of the queue
     */
    virtual uint8_t read() = 0;
    
    /**
     * @brief Get the event that is notified when data is written
     * @return Reference to the data available event
     */
    virtual const sc_core::sc_event& data_available_event() const = 0;
};

/**
 * @brief Thread-safe queue channel using async_request_update()
 * 
 * This channel allows external threads to safely write data that will be
 * processed by SystemC threads. It uses async_request_update() to ensure
 * thread-safe communication between non-SystemC threads and the SystemC kernel.
 */
class thread_safe_queue_channel : public sc_core::sc_prim_channel, 
                                   public thread_safe_queue_if {
public:
    /**
     * @brief Constructor
     * @param name Channel name for SystemC hierarchy
     */
    explicit thread_safe_queue_channel(const char* name = sc_core::sc_gen_unique_name("thread_safe_queue"));
    
    /**
     * @brief Destructor
     */
    virtual ~thread_safe_queue_channel() = default;
    
    /**
     * @brief Write data to the queue from any thread
     * 
     * This method is thread-safe and can be called from external threads.
     * It uses async_request_update() to schedule the update in the SystemC kernel.
     * 
     * @param data Byte to write to the queue
     */
    void write(uint8_t data) override;
    
    /**
     * @brief Check if queue has data
     * @return true if queue is not empty
     */
    bool has_data() const override;
    
    /**
     * @brief Read data from the queue
     * 
     * Should only be called from SystemC threads when has_data() returns true.
     * 
     * @return Byte from the front of the queue
     */
    uint8_t read() override;
    
    /**
     * @brief Get the event that is notified when data is available
     * @return Reference to the data available event
     */
    const sc_core::sc_event& data_available_event() const override;

protected:
    /**
     * @brief Update method called by SystemC kernel
     * 
     * This is called in the update phase of the delta cycle after
     * async_request_update() is invoked. It moves data from the
     * pending queue to the main queue and notifies the event.
     */
    void update() override;

private:
    std::queue<uint8_t> m_queue;         ///< Main queue (accessed only in SystemC context)
    std::queue<uint8_t> m_pending_queue; ///< Pending queue (accessed from external threads)
    sc_core::sc_event m_data_event;      ///< Event notified when data is available
    
    // Note: We don't need explicit mutexes because:
    // - m_pending_queue is only accessed in write() (external threads) and update() (SystemC thread)
    // - async_request_update() ensures update() is called in a thread-safe manner
    // - m_queue is only accessed from SystemC threads (read() and update())
};

