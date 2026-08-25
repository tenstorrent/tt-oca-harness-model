/**
 * @file mailbox_unit.h
 * @brief Multi-channel mailbox unit — model of axi_lite_mailbox_unit.sv
 *
 * The RTL instantiates NUM_MAILBOXES independent axi_lite_mailbox blocks behind
 * one AXI-Lite slave port and demultiplexes accesses to them by address. This
 * module does the same for the TLM model: it owns the channels, routes each
 * transaction to the right channel and port, and exposes the two interrupt
 * vectors the subsystem wires up.
 */

#pragma once

#include "mailbox.h"

#include <tlm_utils/simple_initiator_socket.h>

#include <memory>
#include <string>

/**
 * @class mailbox_unit_t
 * @brief NUM_MAILBOXES mailbox channels behind a single target socket
 *
 * Address decode (axil_mailbox_sep_wrap.rdl):
 *
 *   channel m outbound block @ m * 0x1000 + 0x000
 *   channel m inbound  block @ m * 0x1000 + 0x800
 *
 * so the aperture is a flat array of 2 * NUM_MAILBOXES register blocks of
 * 0x800 bytes each, alternating outbound and inbound. Within a block only
 * offsets 0x00..0x4F are mapped; the channel itself rejects the rest.
 *
 * Outbound is port 0 and inbound is port 1 of the underlying channel, matching
 * the RTL's slv_reqs_i({inbound_req, outbound_req}) ordering. The interrupt
 * vectors follow the same convention: irq_o[0] of channel m drives
 * outbound_irq_o[m] and irq_o[1] drives inbound_irq_o[m].
 *
 * @tparam NUM_MAILBOXES Channel count — 8 for SEP, 32 for SMC.
 */
template <unsigned int NUM_MAILBOXES = 8>
class mailbox_unit_t : public sc_module
{
public:
   /// @brief Number of mailbox channels in this unit
   static constexpr unsigned int NUM_CHANNELS = NUM_MAILBOXES;

   /// @brief Address span of one mailbox register block (RTL MAILBOX_SIZE)
   static constexpr uint64_t MAILBOX_SIZE = 0x800;

   /// @brief Number of register blocks in the aperture (one per port per channel)
   static constexpr unsigned int NUM_BLOCKS = 2 * NUM_MAILBOXES;

   /// @brief Total size of the unit's address aperture
   static constexpr uint64_t APERTURE_SIZE = NUM_BLOCKS * MAILBOX_SIZE;

   /// @brief Single AXI-Lite slave port for the whole unit
   tlm_utils::simple_target_socket<mailbox_unit_t, 32> target_socket;

   /// @brief Per-channel outbound interrupts (RTL outbound_interrupt_o)
   sc_out<bool> outbound_irq_o[NUM_MAILBOXES];

   /// @brief Per-channel inbound interrupts (RTL inbound_interrupt_o)
   sc_out<bool> inbound_irq_o[NUM_MAILBOXES];

   /// @brief Active-low asynchronous reset, fanned out to every channel
   sc_in<bool> rst_ni;

   /// @brief Abstract clock frequency input, fanned out to every channel
   sc_in<double> clk_i;

   /// @brief CSML Logger for diagnostic output
   CsmlLogger logger;

   mailbox_unit_t(sc_module_name n, int log_verbosity = CSML_DEFAULT_VERBOSITY)
       : sc_module(n)
       , target_socket("target_socket")
       , logger()
   {
      logger.setMaxVerbosity(log_verbosity);
      logger.setLogFormat(
          "[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
      logger.setFunctionTrace(false);

      target_socket.register_b_transport(this, &mailbox_unit_t::b_transport);

      for (unsigned int m = 0; m < NUM_MAILBOXES; ++m) {
         const std::string channel_name = "mailbox_" + std::to_string(m);
         m_channel[m] = std::make_unique<mailbox_ip>(channel_name.c_str(), log_verbosity);

         m_channel[m]->rst_ni(rst_ni);
         m_channel[m]->clk_i(clk_i);
         m_channel[m]->irq_o[0](outbound_irq_o[m]);
         m_channel[m]->irq_o[1](inbound_irq_o[m]);

         const std::string out_name = "isock_outbound_" + std::to_string(m);
         const std::string in_name = "isock_inbound_" + std::to_string(m);
         m_block_socket[2 * m] = std::make_unique<block_socket>(out_name.c_str());
         m_block_socket[2 * m + 1] = std::make_unique<block_socket>(in_name.c_str());

         m_block_socket[2 * m]->bind(m_channel[m]->socket0);
         m_block_socket[2 * m + 1]->bind(m_channel[m]->socket1);
      }

      CSML_INFO(2, logger) << "Mailbox unit instantiated with " << NUM_MAILBOXES
                           << " channels, aperture 0x" << std::hex << APERTURE_SIZE
                           << std::dec;
   }

   /// @brief Access a channel, for testbenches that need to reach past the decode
   mailbox_ip& channel(unsigned int m) { return *m_channel[m]; }

private:
   typedef tlm_utils::simple_initiator_socket<mailbox_unit_t, 32> block_socket;

   std::unique_ptr<mailbox_ip> m_channel[NUM_MAILBOXES];
   std::unique_ptr<block_socket> m_block_socket[NUM_BLOCKS];

   /**
    * @brief Route a transaction to the addressed channel and port
    *
    * The channel sees a block-relative address, so its own decode and access
    * rules apply unchanged regardless of which channel was selected.
    */
   void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
   {
      const uint64_t addr = trans.get_address();
      const unsigned int block = static_cast<unsigned int>(addr / MAILBOX_SIZE);

      if (block >= NUM_BLOCKS) {
         CSML_WARN(1, logger) << "Access outside mailbox unit aperture at 0x"
                              << std::hex << addr << std::dec;
         trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
         return;
      }

      trans.set_address(addr % MAILBOX_SIZE);
      (*m_block_socket[block])->b_transport(trans, delay);
      trans.set_address(addr);
   }
};

/// @brief SEP instantiates eight channels (sep_pkg::NUM_MAILBOXES)
typedef mailbox_unit_t<8> mailbox_unit;
