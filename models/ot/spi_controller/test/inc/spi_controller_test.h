
#pragma once
#include "spi_controller_basetest.h"
#include "spi_controller_interface.h"
#include "csml_logger.h"

/// =============================================================================
/// Dummy SPI Interface Implementation (for test model)
/// =============================================================================
/**
 * @class spi_if_dummy
 * @brief Test class for SPI Controller
 */
class spi_if_dummy : public spi_if
{
private:

    /// Data capture and response buffers for testing
    std::vector<uint8_t> m_rx_buffer;        /// Pre-loaded RX data to return to master
    std::vector<uint8_t> m_tx_captured;      /// Captured TX data from master
    size_t m_rx_buffer_index;                /// Current read position in RX buffer

    /// Statistics for verification
    uint32_t m_transaction_count;
    uint32_t m_total_tx_bytes;
    uint32_t m_total_rx_bytes;
    uint8_t m_last_csid;
    bool m_last_csaat;

public:
    /// CSML Logger instance
    CsmlLogger logger;

    /**
     * @brief Constructor for the dummy SPI interface
     */
    spi_if_dummy() : m_rx_buffer_index(0),
                     m_transaction_count(0),
                     m_total_tx_bytes(0),
                     m_total_rx_bytes(0),
                     m_last_csid(0),
                     m_last_csaat(false) {
        // Initialize logger
        logger.setMaxVerbosity(3);
        logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
        logger.setFunctionTrace(false);
    }

    /**
     * @brief Process an SPI transaction
     * @param segment SPI segment configuration
     * @param config SPI configuration parameters
     * @param tx_data Pointer to transmit data buffer
     * @param rx_data Pointer to receive data buffer
     * @return true if transaction succeeded, false otherwise
     */
    virtual bool spi_transaction(const spi_segment_t& segment,
                                  const spi_config_t& config,
                                  const uint8_t* tx_data,
                                  uint8_t* rx_data) override
    {
        CSML_INFO(2, logger) << "[SPI_IF_DUMMY] SPI transaction called: "
                  << "len=" << segment.len
                  << ", speed=" << (int)segment.speed
                  << ", direction=" << (int)segment.direction
                  << ", csid=" << (int)segment.csid
                  << ", csaat=" << segment.csaat << std::endl;

        CSML_DEBUG(3, logger) << "[SPI_IF_DUMMY] Config: "
                  << "clkdiv=" << config.clkdiv << std::endl;

        // Update statistics
        m_transaction_count++;
        m_last_csid = segment.csid;
        m_last_csaat = segment.csaat;

        // Handle TX data (capture from master)
        if (segment.direction == spi_direction_e::TX_ONLY ||
            segment.direction == spi_direction_e::BIDIR) {
            CSML_DEBUG(3, logger) << "[SPI_IF_DUMMY] TX data: ";
            for (uint16_t i = 0; i < segment.len; i++) {
                CSML_DEBUG(3, logger) << "0x" << std::hex << (int)tx_data[i] << std::dec << " ";
                m_tx_captured.push_back(tx_data[i]);  /// Capture for verification
            }
            CSML_DEBUG(3, logger) << std::endl;
            m_total_tx_bytes += segment.len;
        }

        // Handle RX data (return to master)
        if (segment.direction == spi_direction_e::RX_ONLY ||
            segment.direction == spi_direction_e::BIDIR) {
            CSML_DEBUG(3, logger) << "[SPI_IF_DUMMY] RX data: ";
            for (uint16_t i = 0; i < segment.len; i++) {
                // Priority 1: Use pre-loaded RX buffer if available
                if (m_rx_buffer_index < m_rx_buffer.size()) {
                    rx_data[i] = m_rx_buffer[m_rx_buffer_index++];
                }
                // Priority 2: Echo TX data + 1 for BIDIR
                else if (segment.direction == spi_direction_e::BIDIR && tx_data) {
                    rx_data[i] = tx_data[i] + 1;
                }
                // Priority 3: Default test pattern
                else {
                    rx_data[i] = 0xA5 + i;
                }
                CSML_DEBUG(3, logger) << "0x" << std::hex << (int)rx_data[i] << std::dec << " ";
            }
            CSML_DEBUG(3, logger) << std::endl;
            m_total_rx_bytes += segment.len;
        }

        CSML_DEBUG(2, logger) << "[SPI_IF_DUMMY] Transaction complete" << std::endl;

        return true;
    }

    /// =========================================================================
    /// Test Helper Methods
    /// =========================================================================

    /**
     * @brief Load RX data that will be returned to master on next transaction(s)
     * @param data Pointer to data buffer
     * @param len Length of data in bytes
     */
    void load_rx_data(const uint8_t* data, size_t len) {
        m_rx_buffer.clear();
        m_rx_buffer.assign(data, data + len);
        m_rx_buffer_index = 0;
        CSML_DEBUG(2, logger) << "[SPI_IF_DUMMY] Loaded " << len << " bytes into RX buffer" << std::endl;
    }

    /**
     * @brief Load RX data from a vector that will be returned to master
     * @param data Vector containing data to load
     */
    void load_rx_data(const std::vector<uint8_t>& data) {
        m_rx_buffer = data;
        m_rx_buffer_index = 0;
        CSML_DEBUG(2, logger) << "[SPI_IF_DUMMY] Loaded " << data.size() << " bytes into RX buffer" << std::endl;
    }

    /**
     * @brief Get captured TX data from master (for verification in tests)
     * @return Reference to captured TX data vector
     */
    const std::vector<uint8_t>& get_captured_tx_data() const {
        return m_tx_captured;
    }

    /**
     * @brief Clear captured TX data
     */
    void clear_capture() {
        m_tx_captured.clear();
        CSML_DEBUG(2, logger) << "[SPI_IF_DUMMY] Cleared TX capture buffer" << std::endl;
    }

    /**
     * @brief Get transaction count statistic
     * @return Number of transactions processed
     */
    uint32_t get_transaction_count() const { return m_transaction_count; }

    /**
     * @brief Get total TX bytes statistic
     * @return Total number of bytes transmitted
     */
    uint32_t get_total_tx_bytes() const { return m_total_tx_bytes; }

    /**
     * @brief Get total RX bytes statistic
     * @return Total number of bytes received
     */
    uint32_t get_total_rx_bytes() const { return m_total_rx_bytes; }

    /**
     * @brief Get last chip select ID used
     * @return Last CSID value
     */
    uint8_t get_last_csid() const { return m_last_csid; }

    /**
     * @brief Get last CSAAT flag value
     * @return Last CSAAT value
     */
    bool get_last_csaat() const { return m_last_csaat; }

    /**
     * @brief Reset all state
     */
    void reset() {
        m_rx_buffer.clear();
        m_tx_captured.clear();
        m_rx_buffer_index = 0;
        m_transaction_count = 0;
        m_total_tx_bytes = 0;
        m_total_rx_bytes = 0;
        m_last_csid = 0;
        m_last_csaat = false;
        CSML_DEBUG(2, logger) << "[SPI_IF_DUMMY] Reset complete" << std::endl;
    }
};


/// =============================================================================
/// SPI Host Test Class
/// =============================================================================
/**
 * @class spi_controller_test
 * @brief Test class for SPI Controller
 */
class spi_controller_test : public spi_controller_basetest
{
public:
   /// Complementary port declarations (opposite of model ports)
   /// Model has sc_port<spi_if>, test has sc_export<spi_if>
   sc_export<spi_if> spi_master;



   /// Interrupt inputs (complement of model's outputs)
   sc_in<bool> error_irq;
   sc_in<bool> spi_event_irq;

   /// DMA trigger input (complement of model's output)
   sc_in<bool> dma_trigger;

   /// Clock output (complement of model's input)
   sc_out<bool> clk_i;

   /// Reset output (complement of model's input)
   sc_out<bool> rst_ni;

   /// Dummy interface implementation (public for testbench access)
   spi_if_dummy m_spi_if_impl;

   /// CSML Logger instance (mutable to allow logging in const functions)
   mutable CsmlLogger logger;

   /**
    * @brief Test class for SPI Controller
    * @param name SystemC module name
    */
   spi_controller_test(sc_module_name name) : spi_controller_basetest(name),
                                        spi_master("spi_master"),
                                        error_irq("error_irq"),
                                        spi_event_irq("spi_event_irq"),
                                        dma_trigger("dma_trigger"),
                                        clk_i("clk_i"),
                                        rst_ni("rst_ni")
   {
      /// Initialize logger
      logger.setMaxVerbosity(3);
      logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
      logger.setFunctionTrace(false);

      /// Bind export to dummy implementation
      spi_master.bind(m_spi_if_impl);
   }

   /**
    * @brief Read an 8-bit register
    * @param offset Register offset
    * @param read_value Reference to store read value
    */
   void register_read_8(unsigned int offset, uint8_t &read_value);

   /**
    * @brief Write an 8-bit register
    * @param offset Register offset
    * @param write_value Value to write
    */
   void register_write_8(unsigned int offset, uint8_t write_value);

   /**
    * @brief Read a 32-bit register
    * @param offset Register offset
    * @param read_value Reference to store read value
    */
   void read_register_32(unsigned int offset, uint32_t &read_value);

   /**
    * @brief Write a 32-bit register
    * @param offset Register offset
    * @param write_value Value to write
    */
   void write_register_32(unsigned int offset, uint32_t write_value);

   /**
    * @brief Write a 32-bit register with custom byte enables (for testing ACCESSINVAL)
    * @param offset Register offset
    * @param write_value Value to write
    * @param be0 Byte enable for byte 0
    * @param be1 Byte enable for byte 1
    * @param be2 Byte enable for byte 2
    * @param be3 Byte enable for byte 3
    */
   void write_register_32_with_byte_enable(unsigned int offset, uint32_t write_value,
                                            unsigned char be0, unsigned char be1,
                                            unsigned char be2, unsigned char be3);

   /**
    * @brief Assertion helper function
    * @param test_name Name of the test
    * @param expected Expected value
    * @param actual Actual value
    */
   void assert_register_value(const char* test_name, uint32_t expected, uint32_t actual);

   /**
    * @brief Helper function to toggle reset
    */
   void toggle_reset();

   /**
    * @brief End of elaboration callback
    */
   void end_of_elaboration();

   /// =========================================================================
   /// SPI Slave Access Helper Methods (for convenient test access)
   /// =========================================================================

   /**
    * @brief Load RX data into slave that will be returned on next SPI transaction
    * @param data Pointer to data buffer
    * @param len Length of data in bytes
    */
   void load_slave_rx_data(const uint8_t* data, size_t len) {
      m_spi_if_impl.load_rx_data(data, len);
   }

   /**
    * @brief Load RX data into slave from a vector
    * @param data Vector containing data to load
    */
   void load_slave_rx_data(const std::vector<uint8_t>& data) {
      m_spi_if_impl.load_rx_data(data);
   }

   /**
    * @brief Get TX data captured by slave from master
    * @return Reference to captured TX data vector
    */
   const std::vector<uint8_t>& get_slave_captured_tx_data() const {
      return m_spi_if_impl.get_captured_tx_data();
   }

   /**
    * @brief Verify slave received expected TX data from master
    * @param expected Expected TX data vector
    * @return true if data matches, false otherwise
    */
   bool verify_slave_received_tx(const std::vector<uint8_t>& expected) const {
      const auto& captured = m_spi_if_impl.get_captured_tx_data();
      bool match = (captured == expected);

      if (match) {
         CSML_INFO(2, logger) << "[VERIFY] Slave RX matches expected TX data ("
                   << captured.size() << " bytes)" << std::endl;
      } else {
         CSML_ERROR(2, logger) << "[VERIFY FAIL] Slave RX mismatch!" << std::endl;
         CSML_ERROR(2, logger) << "  Expected: ";
         for (auto b : expected) CSML_ERROR(2, logger) << "0x" << std::hex << (int)b << " ";
         CSML_ERROR(2, logger) << std::endl << "  Captured: ";
         for (auto b : captured) CSML_ERROR(2, logger) << "0x" << std::hex << (int)b << " ";
         CSML_ERROR(2, logger) << std::dec << std::endl;
      }

      return match;
   }

   /**
    * @brief Get slave transaction count statistic
    * @return Number of transactions processed by slave
    */
   uint32_t get_slave_transaction_count() const {
      return m_spi_if_impl.get_transaction_count();
   }

   /**
    * @brief Get slave TX bytes statistic
    * @return Total number of bytes transmitted by slave
    */
   uint32_t get_slave_tx_bytes() const {
      return m_spi_if_impl.get_total_tx_bytes();
   }

   /**
    * @brief Get slave RX bytes statistic
    * @return Total number of bytes received by slave
    */
   uint32_t get_slave_rx_bytes() const {
      return m_spi_if_impl.get_total_rx_bytes();
   }

   /**
    * @brief Clear slave state between tests
    */
   void clear_slave_state() {
      m_spi_if_impl.reset();
   }



   /**
    * @brief Destructor
    */
   ~spi_controller_test() {}
};