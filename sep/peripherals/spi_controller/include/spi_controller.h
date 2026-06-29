/**
 * @file spi_controller.h
 * @brief Main SPI Controller TLM model class definition
 *
 * This header defines the complete SPI Controller TLM model including:
 * - Main spi_controller class with ports, configuration, and transaction processing
 * - Hardware register access via TLM target socket
 * - Command queue and FIFO management for SPI transactions
 * - Support for Standard/Dual/Quad SPI modes
 * - Multi-device support with configurable chip selects
 */

#include "spi_controller_base.h"
#include "spi_controller_interface.h"
#include "csml_logger.h"
#include "csml_parameter.h"
#include <tlm_utils/tlm_quantumkeeper.h>
#include <queue>
#include <deque>

class spi_controller_ip : public spi_controller_base
{
public:
   SC_HAS_PROCESS(spi_controller_ip);

   /// Port declarations for custom interfaces
   /// SPI Master interface (initiator) - model initiates SPI transactions
   sc_port<spi_if> spi_master;

   /// Interrupt outputs
   sc_out<bool> error_irq;  ///< Error interrupt output for programming violations
   sc_out<bool> spi_event_irq;  ///< Event interrupt output for FIFO watermarks and status changes

   /// DMA trigger output
   sc_out<bool> dma_trigger;  ///< DMA trigger asserted when RX/TX FIFOs reach watermarks

   /// Reset input (active-low asynchronous reset)
   sc_in<bool> rst_ni;  ///< Active-low asynchronous reset input

   /// Clock input (standard SystemC clock signal)
   sc_in<bool> clk_i;  ///< Functional clock input for SCK generation

   /// CSML Logger instance
   CsmlLogger logger;  ///< Logger for debug and tracing

   /// ==========================================================================
   /// Configuration Parameters (csml_param — portable across CCI)
   /// ==========================================================================
   csml_param<uint32_t> NumCS;              ///< Number of chip select lines
   csml_param<uint32_t> TxDepth;            ///< TX FIFO depth in 32-bit words
   csml_param<uint32_t> RxDepth;            ///< RX FIFO depth in 32-bit words
   csml_param<bool>     ByteOrder;          ///< true=Little-Endian, false=Big-Endian
   csml_param<uint32_t> CmdDepth;           ///< Command FIFO depth
   csml_param<double>   ClkPeriodNs;        ///< Clock period in nanoseconds (default: 10ns = 100MHz)
   csml_param<double>   TimeKeeperQuantumNs; ///< Time keeper quantum in ns (0 = disabled)
   csml_param<int>      verbosity;  ///< Logging verbosity: 0=error, 1=warn, 2=info, 3=debug

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif
   spi_controller_ip(sc_module_name n, int log_verbosity = CSML_DEFAULT_VERBOSITY);

   /**
    * @brief SystemC end_of_elaboration callback for initialization
    */
   void end_of_elaboration();

   /**
    * @brief Public accessors - work with both CCI and non-CCI configurations
    */

   /**
    * @brief Get number of chip select lines
    * @return Number of chip select lines configured
    */
   uint32_t get_num_cs()    { return NumCS.get_param_value(); }
   uint32_t get_tx_depth()  { return TxDepth.get_param_value(); }
   uint32_t get_rx_depth()  { return RxDepth.get_param_value(); }
   bool     get_byte_order() { return ByteOrder.get_param_value(); }
   uint32_t get_cmd_depth() { return CmdDepth.get_param_value(); }
   sc_time  get_clk_period() { return sc_time(ClkPeriodNs.get_param_value(), SC_NS); }
private:
   /**
    * @brief FSM states for TLM abstraction
    */
   enum class fsm_state_e {
      IDLE,              ///< Ready to accept new commands, CSB deasserted
                         ///< STATUS.READY=1, STATUS.ACTIVE=0
      ACTIVE,            ///< Actively processing transaction segment
                         ///< STATUS.READY=0, STATUS.ACTIVE=1
      IDLE_CSB_ACTIVE    ///< Ready for next segment, CSB held low (CSAAT support)
                         ///< STATUS.READY=1, STATUS.ACTIVE=0
                         ///< Used in multi-segment transactions when CSAAT=1
   };

   /**
    * @brief Reset process handler
    */
   void reset_process();

   /**
    * @brief Register all register callback handlers
    */
   void register_callbacks();

   /**
    * @brief Register callback handlers
    */

   /**
    * @brief Handle write to INTR_STATUS register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_INTR_STATUS(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to INTR_ENABLE register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_INTR_ENABLE(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to INTR_TEST register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_INTR_TEST(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to CTRL register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_CTRL(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to CMD register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_CMD(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle read from STATUS register
    * @param value Reference to store read value
    * @param read_bit_mask Bit mask for read operation
    * @return True if read succeeded, false otherwise
    */
   bool handle_read_STATUS(uint32_t& value, uint32_t read_bit_mask);

   /**
    * @brief Handle write to ERROR_STATUS register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_ERROR_STATUS(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to ERROR_ENABLE register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_ERROR_ENABLE(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to EVENT_ENABLE register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_EVENT_ENABLE(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle write to TXDATA register
    * @param value Value to write
    * @param byte_enable Byte enable mask
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_TXDATA(uint32_t value, uint8_t byte_enable, uint32_t write_bit_mask);

   /**
    * @brief Handle write to CFG register
    * @param value Value to write
    * @param write_bit_mask Bit mask for write operation
    * @return True if write succeeded, false otherwise
    */
   bool handle_write_CFG(uint32_t value, uint32_t write_bit_mask);

   /**
    * @brief Handle read from CFG register
    * @param value Reference to store read value
    * @param read_bit_mask Bit mask for read operation
    * @return True if read succeeded, false otherwise
    */
   bool handle_read_CFG(uint32_t& value, uint32_t read_bit_mask);

   /**
    * @brief Handle read from RXDATA register
    * @param value Reference to store read value
    * @param read_bit_mask Bit mask for read operation
    * @return True if read succeeded, false otherwise
    */
   bool handle_read_RXDATA(uint32_t& value, uint32_t read_bit_mask);

   /**
    * @brief Helper methods
    */

   /**
    * @brief Update error_irq output based on error conditions
    */
   void update_error_irq();

   /**
    * @brief Update spi_event_irq output based on event conditions
    */
   void update_spi_event_irq();

   /**
    * @brief Update error interrupt state based on ERROR_STATUS and ERROR_ENABLE
    */
   void update_error_interrupt_state();

   /**
    * @brief Update DMA trigger output based on FIFO watermarks
    */
   void update_dma_trigger();

   /**
    * @brief Update event interrupt state based on EVENT_ENABLE
    */
   void update_event_interrupt_state();

   /**
    * @brief Level-sensitive recompute of INTR_STATUS.spi_event per RTL equation:
    *   spi_event_intr = (|(event_vector & event_mask) || INTR_TEST.spi_event) && INTR_ENABLE.spi_event
    * Clears as well as sets — call whenever EVENT_ENABLE, INTR_ENABLE, or INTR_TEST changes.
    */
   void update_spi_event_intr_status();

   /**
    * @brief Thread to process SPI transactions
    */
   void spi_transaction_thread();

   /**
    * @brief Set FSM state and update associated status flags
    * @param new_state New FSM state to transition to
    */
   void set_fsm_state(fsm_state_e new_state);

   /**
    * @brief CSID validation helper
    * @param csid Chip select ID to validate
    * @param context Context string for error reporting
    * @return True if CSID is valid, false otherwise
    */
   bool validate_csid(uint32_t csid, const char* context);

   /**
    * @brief FIFO helper methods
    */

   /**
    * @brief Push data to TX FIFO
    * @param data 32-bit data word to push
    * @return True if push succeeded, false if FIFO full
    */
   bool tx_fifo_push(uint32_t data);

   /**
    * @brief Pop data from TX FIFO
    * @param data Reference to store popped data
    * @return True if pop succeeded, false if FIFO empty
    */
   bool tx_fifo_pop(uint32_t& data);

   /**
    * @brief Push data to RX FIFO
    * @param data 32-bit data word to push
    * @return True if push succeeded, false if FIFO full
    */
   bool rx_fifo_push(uint32_t data);

   /**
    * @brief Pop data from RX FIFO
    * @param data Reference to store popped data
    * @return True if pop succeeded, false if FIFO empty
    */
   bool rx_fifo_pop(uint32_t& data);

   /**
    * @brief Flush both TX and RX FIFOs
    */
   void flush_fifos();

   /**
    * @brief Get current TX FIFO depth
    * @return Number of 32-bit words in TX FIFO
    */
   uint32_t get_tx_fifo_depth() const { return m_tx_fifo.size(); }

   /**
    * @brief Get current RX FIFO depth
    * @return Number of 32-bit words in RX FIFO
    */
   uint32_t get_rx_fifo_depth() const { return m_rx_fifo.size(); }

   /**
    * @brief Check if TX FIFO is full
    * @return True if TX FIFO is full, false otherwise
    */
   bool is_tx_fifo_full() { return m_tx_fifo.size() >= get_tx_depth(); }

   /**
    * @brief Check if TX FIFO is empty
    * @return True if TX FIFO is empty, false otherwise
    */
   bool is_tx_fifo_empty() const { return m_tx_fifo.empty(); }

   /**
    * @brief Check if RX FIFO is full
    * @return True if RX FIFO is full, false otherwise
    */
   bool is_rx_fifo_full() { return m_rx_fifo.size() >= get_rx_depth(); }

   /**
    * @brief Check if RX FIFO is empty
    * @return True if RX FIFO is empty, false otherwise
    */
   bool is_rx_fifo_empty() const { return m_rx_fifo.empty(); }

   /**
    * @brief Check if command queue is full
    * @return True if command queue is full, false otherwise
    */
   bool is_cmd_queue_full() { return m_command_queue.size() >= get_cmd_depth(); }

   /**
    * @brief Get current command queue depth
    * @return Number of commands in queue
    */
   uint32_t get_cmd_queue_depth() const { return m_command_queue.size(); }

   /**
    * @brief ByteOrder helpers
    */

   /**
    * @brief Pack bytes into 32-bit word according to ByteOrder setting
    * @param bytes Pointer to byte array to pack
    * @param count Number of bytes to pack (1-4)
    * @return Packed 32-bit word
    */
   uint32_t pack_word(const uint8_t* bytes, size_t count);

   /**
    * @brief Unpack 32-bit word into bytes according to ByteOrder setting
    * @param word 32-bit word to unpack
    * @param bytes Pointer to byte array to store unpacked data
    * @param count Number of bytes to unpack (1-4)
    */
   void unpack_word(uint32_t word, uint8_t* bytes, size_t count);

   /**
    * @brief Timing calculation
    */

   /**
    * @brief Calculate segment delay based on configuration
    * @param segment SPI segment descriptor
    * @param config SPI configuration
    * @return Calculated delay in nanoseconds
    */
   double calculate_segment_delay(const spi_segment_t& segment, const spi_config_t& config);

   /**
    * @brief Transaction processing helper
    * @param segment SPI segment descriptor
    * @param config SPI configuration
    * @return True on success, false on error
    */
   bool process_single_transaction(const spi_segment_t& segment, const spi_config_t& config);

   /// FSM state variables
   fsm_state_e m_fsm_state;  ///< Current FSM state

   /// Transaction tracking
   bool m_transaction_pending;  ///< Flag indicating if a transaction is pending
   spi_segment_t m_current_segment;  ///< Current segment being processed
   spi_config_t m_current_config;  ///< Current configuration for the segment

   /// Command queue for segment descriptors
   std::queue<std::pair<spi_segment_t, spi_config_t>> m_command_queue;  ///< Queue of pending commands

   /// TX/RX FIFOs (32-bit words)
   std::deque<uint32_t> m_tx_fifo;  ///< Transmit FIFO
   std::deque<uint32_t> m_rx_fifo;  ///< Receive FIFO

   /// Time keeper for temporal decoupling support (TLM standard quantum keeper)
   tlm_utils::tlm_quantumkeeper m_time_keeper;  ///< TLM quantum keeper instance for temporal decoupling

   /// Event for transaction completion
   sc_event m_transaction_event;  ///< Event signaled when transaction completes

   /// Events for signal updates (to avoid multiple driver conflicts)
   sc_event m_update_signals_event;  ///< Event to trigger signal update method

   /// Event for RX FIFO space available (signaled when RXDATA is read)
   sc_event m_rx_space_available_event;  ///< Event for RX FIFO space availability

   /// Event signaled when dma_trigger transitions HIGH→LOW (TX FIFO reached watermark).
   /// Used by process_single_transaction to wait for a full DMA chunk before consuming,
   /// ensuring proper LOW→HIGH rising-edge visibility for the DMA handshake monitor.
   sc_event m_tx_fifo_at_watermark;  ///< TX FIFO-at-watermark event (trigger HIGH→LOW)

   /// Event signaled whenever a word is pushed to the TX FIFO (tx_fifo_push succeeds).
   /// Used to wake the TX stall wait when TX_WATERMARK=0 (no DMA watermark configured),
   /// mirroring RTL behavior where the SPI clock resumes as soon as TXDATA is written.
   sc_event m_tx_data_available;  ///< TX FIFO non-empty event (any push)

   /// Event signaled when dma_trigger transitions HIGH→LOW after the RX FIFO was above
   /// watermark and DMA has drained it back below.  SPI uses this to pace chunked
   /// RX-FIFO pushes so that each DMA chunk sees a clean LOW→HIGH rising edge.
   sc_event m_rx_fifo_drained_event;  ///< RX FIFO drained-below-watermark event

   /**
    * @brief SC_METHOD to handle all signal writes (single driver)
    */
   void update_output_signals_method();

   /// Previous STATUS values for edge detection
   bool m_prev_ready;  ///< Previous READY status for edge detection
   bool m_prev_active;  ///< Previous ACTIVE status for edge detection
   bool m_prev_txempty;  ///< Previous TXEMPTY status for edge detection
   bool m_prev_rxfull;  ///< Previous RXFULL status for edge detection
   bool m_prev_txwm;  ///< Previous TX watermark state for edge detection
   bool m_prev_rxwm;  ///< Previous RX watermark state for edge detection

   /// INTR_TEST state tracking for write-0-to-release semantics
   bool m_intr_test_error_forced;  ///< Tracks if error interrupt is test-forced
   bool m_intr_test_spi_event_forced;  ///< Tracks if spi_event interrupt is test-forced

   /// ====================================================================
   /// ====================================================================
   /// CONFIGOPTS Multi-Register Support (Shadow Array)
   /// ====================================================================
   /// CONFIGOPTS is a "pure storage register" that stores per-device timing
   /// configuration. We maintain a shadow array indexed by CSID.
   /// When COMMAND is written, we capture current CONFIGOPTS value into
   /// m_configopts_array[CSID] and use it for the transaction.
   ///
   /// Software workflow:
   ///   1. Write CSID = device_id
   ///   2. Write CONFIGOPTS = timing_config
   ///   3. Write COMMAND -> captures CONFIGOPTS into array[device_id]
   /// ====================================================================
   std::vector<uint32_t> m_configopts_array;  ///< Shadow array [0..NumCS-1]
};
