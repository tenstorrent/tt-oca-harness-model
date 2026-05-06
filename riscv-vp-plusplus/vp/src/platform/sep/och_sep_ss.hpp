#include <systemc.h>
#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <tlm_utils/simple_target_socket.h>
#include <tlm.h>
#include "sep_memory.h"
#include <dma.h>
#include "uart_with_terminal.h"
#include "gpio.h"
#include "hmac.h"
#include "kmac.h"
#include "otbn.h"
#include "otbn_interfaces.h"
#include "spi_controller.h"
#include "spi_controller_interface.h"
#include "spi_flash.h"
#include "crng.h"
#include "keymgr_tt.h"
#include <openssl/rand.h>
#include <cstring>
#include "aes.h"
#include "mailbox.h"
#include "mailbox_host_stub.h"
#include "mailbox_bridge.h"
#include "aon_timer.h"
#include "sep_efuse.h"
#include "lc_ctrl.h"
#include "xspi_ctrl.h"
#include "xspi_target_sc_wrapper.h"
#include "AVBBus.h"
#include "entropy_src.h"
#include "edn.h"
#include "fe310_plic.h"
#include "core/common/clint.h"
#include "core/rv32/elf_loader.h"
#include "net_trace.h"
#include "util/options.h"
#include "platform/common/bus.h"
#include "HartConfig.hpp"
#include "VeeR-ISSTlm.hpp"
#include "Args.hpp"

// SimpleBus template parameters:
//   INIT_COUNT: total number of bus initiators
//   Initiators: riscv, dma, dma_sys_adapter
const unsigned int INIT_COUNT          = 4;
//   TARG_COUNT: total number of bus targets
//   Targets: sram, rom, plic, clint, dma, uart, gpio, hmac, otbn, itcm, dtcm, stdout, spi_controller, kmac, csrng, aes, mailbox, aon_timer, keymgr_mb, keymgr_kpvlp, sep_efuse, lc_ctrl, xspi_ctrl, avbbus, entropy_src, edn
const unsigned int TARG_COUNT          = 26;
//   Current sources used are listed below with the irq number:
//   Remaining inputs are left tied-off via unused_irq_signal.
const unsigned int PLIC_NUM_INTERRUPTS = 30;
const unsigned int UART_IRQ            = 3;
const unsigned int SPI_ERROR_IRQ       = 4;
const unsigned int AON_WKUP_IRQ        = 5;
const unsigned int AON_WDOG_IRQ        = 6;
const unsigned int XSPI_IRQ            = 7;
const unsigned int GPIO_IRQ            = 8;
const unsigned int KEYMGR_IRQ          = 9;
const unsigned int SPI_EVENT_IRQ       = 10;
const unsigned int HMAC_DONE_IRQ       = 11;
const unsigned int HMAC_FIFO_EMPTY_IRQ = 12;
const unsigned int HMAC_HMAC_ERR_IRQ   = 13;
const unsigned int KMAC_IRQ            = 14;
const unsigned int OTBN_IRQ            = 15;
const unsigned int DMA_DONE_IRQ        = 16;
const unsigned int DMA_CHUNK_DONE_IRQ  = 17;
const unsigned int DMA_ERROR_IRQ       = 18;
const unsigned int MAILBOX_IRQ0        = 29;
const unsigned int MAILBOX_IRQ1        = 20;
const unsigned int CS_CMD_REQ_DONE     = 21;
const unsigned int CS_ENTROPY_REQ      = 22;
const unsigned int CS_HW_INST_EXC      = 23;
const unsigned int CS_FATAL_ERR        = 24;
const unsigned int ENTROPY_SRC_IRQ     = 25;
const unsigned int EDN_CMD_REQ_DONE    = 26;
const unsigned int EDN_FATAL_ERR       = 27;

using namespace rv32;
namespace po = boost::program_options;

// GPIO bridge module for loopback testing
// Connects GPIO output back to its input (simple loopback)
class gpio_bridge : public sc_module {
    public:
        sc_in<bool> gpio_out_in;
        sc_out<bool> gpio_in_out;

        void loopback_process() {
            gpio_in_out.write(gpio_out_in.read());
        }

        SC_CTOR(gpio_bridge) {
            SC_METHOD(loopback_process);
            sensitive << gpio_out_in;
        }
};


// Stdout device - simple memory-mapped output device
// Writes to this device are printed to stdout character by character.
// 4-byte writes whose lower byte matches a testbench mailbox command are
// intercepted and drive the corresponding output signal instead of printing.
// Currently handled commands (matching tb.h / tb_top.sv):
//   0x81 (LOAD_NMI_ADDR) — drives nmi_vec_o with the NMI handler address
class stdout_device : public sc_module {
    public:
        tlm_utils::simple_target_socket<stdout_device> sock;
        // Driven when firmware sends LOAD_NMI_ADDR (0x81) mailbox command.
        // Carries the NMI handler address (upper 24 bits; lower 8 bits are 0).
        sc_core::sc_out<uint32_t> nmi_vec_o;

        stdout_device(sc_module_name name)
            : sc_module(name), sock("sock"), nmi_vec_o("nmi_vec_o") {
            sock.register_b_transport(this, &stdout_device::b_transport);
            sock.register_transport_dbg(this, &stdout_device::transport_dbg);
        }

        void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
            auto cmd = trans.get_command();
            auto ptr = trans.get_data_ptr();
            auto len = trans.get_data_length();

            if (cmd == tlm::TLM_WRITE_COMMAND) {
                if (len == 1) {
                    char c = *reinterpret_cast<char*>(ptr);
                    std::cout << c;
                } else if (len == 4) {
                    // init_stdout.c writes a size_t (4 bytes) where only the LSB contains the character
                    uint32_t value = *reinterpret_cast<uint32_t*>(ptr);
                    char c = static_cast<char>(value & 0xFF);
                    if ((value & 0xFF) == 0x81) {
                        // LOAD_NMI_ADDR mailbox command — upper 24 bits carry the NMI handler address
                        nmi_vec_o.write(value & 0xFFFFFF00);
                    } else {
                        std::cout << c;
                    }
                }
                std::cout.flush();
            }
            // Reads are not supported (or return 0)

            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            (void)delay;  // zero delay
        }

        unsigned transport_dbg(tlm::tlm_generic_payload &trans) {
            auto cmd = trans.get_command();
            auto ptr = trans.get_data_ptr();
            auto len = trans.get_data_length();

            if (cmd == tlm::TLM_WRITE_COMMAND) {
                if (len == 1) {
                    char c = *reinterpret_cast<char*>(ptr);
                    std::cout << c;
                } else if (len == 4) {
                    // init_stdout.c writes a size_t (4 bytes) where only the LSB contains the character
                    uint32_t value = *reinterpret_cast<uint32_t*>(ptr);
                    char c = static_cast<char>(value & 0xFF);
                    if ((value & 0xFF) == 0x81) {
                        // LOAD_NMI_ADDR mailbox command — upper 24 bits carry the NMI handler address
                        nmi_vec_o.write(value & 0xFFFFFF00);
                    } else {
                        std::cout << c;
                    }
                }
                std::cout.flush();
            }

            // Reads are not supported (or return 0)
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return len;
        }
};


// ============================================================================
// OTBN Interface Stubs
// Simple stub implementations for OTBN external interfaces (EDN, OTP)
// Based on models/ot/otbn/test/inc/otbn_test.h
// ============================================================================
// EDN Request Stub - Tracks entropy requests but doesn't provide entropy
class edn_req_stub : public edn_req_if, public sc_module {
    public:
        SC_HAS_PROCESS(edn_req_stub);
        edn_req_stub(sc_module_name name) : sc_module(name) {}

        void request_entropy() override {
            // Stub: Do nothing
        }
};

// EDN Response Stub - Returns no entropy available
class edn_rsp_stub : public edn_rsp_if, public sc_module {
    public:
        SC_HAS_PROCESS(edn_rsp_stub);
        edn_rsp_stub(sc_module_name name) : sc_module(name) {}

        bool entropy_available() override {
            return false; 
        }

        uint32_t get_entropy() override {
            return 0;  
        }
};

// OTP Key Request Stub - Tracks scramble key requests
class otp_key_req_stub : public otp_key_req_if, public sc_module {
    public:
        SC_HAS_PROCESS(otp_key_req_stub);
        otp_key_req_stub(sc_module_name name) : sc_module(name) {}

        void request_scramble_key() override {
            // Stub: Do nothing
        }
};

// OTP Key Response Stub - Returns dummy scramble keys
class otp_key_rsp_stub : public otp_key_rsp_if, public sc_module {
    public:
        SC_HAS_PROCESS(otp_key_rsp_stub);
        otp_key_rsp_stub(sc_module_name name) : sc_module(name) {}

        bool key_available() override {
            return true;  // Pretend key is available
        }

        void get_scramble_key(uint32_t key[4], uint32_t& nonce, uint32_t& seed) override {
            // Return dummy/zero keys
            key[0] = key[1] = key[2] = key[3] = 0;
            nonce = 0;
            seed = 0;
        }
};


// Reset Generation unit: Performs proper reset sequence at simulation start
// Also  takes an input request from aon timer/wdog timer for a system reset request
class reset_generation_unit : public sc_module {

    public:
        sc_out<bool> rst_ni;
        sc_in<bool> reset_req_i;
        sc_event wait_for_reset;
		sc_event start_monitor_ev_;
		sc_event external_reset_ev_;
		bool external_reset_in_progress_ = false;

        SC_HAS_PROCESS(reset_generation_unit);
        void end_of_elaboration() {
            rst_ni.write(true);
        }

        reset_generation_unit(sc_module_name name)
            : sc_module(name)
            , rst_ni("rst_ni")
            , reset_req_i("reset_req_i") {
				SC_THREAD(por_thread);
				SC_THREAD(external_reset_thread);
				SC_METHOD(external_reset_method);
				sensitive << reset_req_i;
				dont_initialize();
            }
        ~reset_generation_unit() {}

    private:
        void do_reset_pulse() {
            std::cout << "[" << sc_core::sc_time_stamp() << "] reset_generation_unit: issuing reset pulse" << std::endl;
            rst_ni->write(false);
            wait(10, SC_NS);
            rst_ni->write(true);
        }

		void por_thread() {
			// Power-on reset pulse
			do_reset_pulse();

			if (reset_req_i.read()) {
				wait(reset_req_i.negedge_event());
			}

			start_monitor_ev_.notify();
		}

		void external_reset_method() {
			if (reset_req_i.read()) {
				external_reset_ev_.notify(SC_ZERO_TIME);
			}
		}

		void external_reset_thread() {
			wait(start_monitor_ev_);
			while (1) {
				wait(external_reset_ev_);
				if (external_reset_in_progress_) {
					continue;
				}
				if (!reset_req_i.read()) {
					continue;
				}
				external_reset_in_progress_ = true;
				std::cout << "[" << sc_core::sc_time_stamp() << "] reset_generation_unit: external reset request asserted" << std::endl;
				do_reset_pulse();
				wait(reset_req_i.negedge_event());
				external_reset_in_progress_ = false;
			}
		}
};


// 64-to-32 bit bus adapter for DMA sys_initiator_socket (64-bit) → SimpleBus (32-bit)
class dma_sys_bus_adapter : public sc_core::sc_module {
    public:
        tlm_utils::simple_target_socket<dma_sys_bus_adapter, 64> tgt;
        tlm_utils::simple_initiator_socket<dma_sys_bus_adapter, 32> ini;

        SC_HAS_PROCESS(dma_sys_bus_adapter);
        dma_sys_bus_adapter(sc_core::sc_module_name n)
            : sc_module(n), tgt("tgt"), ini("ini") {
            tgt.register_b_transport(this, &dma_sys_bus_adapter::b_transport);
        }

    private:
        void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
            ini->b_transport(trans, delay);
        }
};

class xspi_dma_target_stub : public sc_core::sc_module {
    public:
        tlm_utils::simple_target_socket<xspi_dma_target_stub, 64> socket;

        explicit xspi_dma_target_stub(sc_core::sc_module_name n)
            : sc_module(n), socket("socket") {
            socket.register_b_transport(this, &xspi_dma_target_stub::b_transport);
        }

    private:
        void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
            delay = sc_core::SC_ZERO_TIME;
            if (trans.is_read()) {
                unsigned char* p = trans.get_data_ptr();
                const unsigned len = static_cast<unsigned>(trans.get_data_length());
                if (p != nullptr && len > 0) {
                    std::memset(p, 0, len);
                }
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        }
};

class xspi_idle_initiator_stub : public sc_module {
    public:
        tlm_utils::simple_initiator_socket<xspi_idle_initiator_stub, 64> socket;
        explicit xspi_idle_initiator_stub(sc_module_name name)
            : sc_module(name), socket("socket") {}
};

class xspi_por_bootstrap_initiator : public sc_module {
    public:
        tlm_utils::simple_initiator_socket<xspi_por_bootstrap_initiator, 64> socket;
        sc_core::sc_in<bool> rst_n;

        SC_HAS_PROCESS(xspi_por_bootstrap_initiator);

        explicit xspi_por_bootstrap_initiator(sc_module_name name)
            : sc_module(name), socket("socket"), rst_n("rst_n") {
            SC_THREAD(por_thread);
        }

    private:
        void por_thread() {
            if (!rst_n.read()) {
                wait(rst_n.posedge_event());
            }
            issue_por_transaction();
        }

        void issue_por_transaction() {
            xspi_PoR_trans por_ext;
            por_ext.discovery_inhibit   = 0u;
            por_ext.discovery_bank      = 0u;
            por_ext.discovery_num_lines = 1u;
            por_ext.discovery_abnum     = 0u;
            por_ext.discovery_cmd_type  = 0u;
            por_ext.discovery_dummy_cnt = 0u;
            por_ext.discovery_extop_en  = 0u;
            por_ext.boot_en             = 0u;

            tlm::tlm_generic_payload payload;
            uint8_t data_buf[8] = {0};
            payload.set_command(tlm::TLM_WRITE_COMMAND);
            payload.set_address(static_cast<uint64_t>(0));
            payload.set_data_ptr(data_buf);
            payload.set_data_length(8u);
            payload.set_streaming_width(8u);
            payload.set_byte_enable_ptr(nullptr);
            payload.set_byte_enable_length(0u);
            payload.set_dmi_allowed(false);
            payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            payload.set_extension(&por_ext);

            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            socket->b_transport(payload, delay);

            xspi_PoR_trans* removed = nullptr;
            payload.get_extension(removed);
            if (removed != nullptr) {
                payload.clear_extension(removed);
            }
        }
};

namespace {
    // Default options for och_sep_ss(sc_module_name) — main.cpp; delegates to two-arg ctor.
    inline BasicOptions& och_sep_ss_default_options_ref() {
        static BasicOptions o;
        return o;
    }
} // namespace

class och_sep_ss : public sc_module {

    public:
        BasicOptions opt;
        SC_HAS_PROCESS(och_sep_ss);
        ~och_sep_ss() {
                delete sram;
                delete rom;
                delete itcm;
                delete dtcm;
                delete stdout_dev;
                delete dma;
                delete dma_sys_adapter;
                delete xspi_dma_stub;
                delete uart;
                delete gpio;
                delete hmac;
                delete kmac;
                delete otbn;
                delete csrng;
				delete aes;
                delete mailbox;
                delete mbox_host;
                delete mbox_bridge;
                delete aon_timer;
				delete sep_efuse;
                delete lc_ctrl;
                delete avbbus;
				delete entropy_src;
				delete edn;
                delete clint;
                delete plic;
                for (auto* p : bus->ports) {
                    delete p;
                }
                delete bus;
                delete rsu_module;
                delete gpio_loopback;
                delete edn_rnd_req_stub_inst;
                delete edn_rnd_rsp_stub_inst;
                delete edn_urnd_req_stub_inst;
                delete otp_key_req_stub_inst;
                delete keymgr;
                delete spi_device;
                delete spi_controller;
                delete xspi_flash;
                delete xspi_ctrl;
                delete xspi_axi_idle_ini;
                delete xspi_por_idle_ini;
            }

        // Sub-module of OCH SEP subsystem
        // Memory
        SEPMemory               *sram;
        SEPMemory               *rom;
        SEPMemory               *itcm;
        SEPMemory               *dtcm;
        // STDOUT 
        stdout_device           *stdout_dev;
        // DMA
        dma_model               *dma;
        dma_sys_bus_adapter     *dma_sys_adapter;
        // UART 
        UART_with_terminal      *uart;
        // GPIO
        gpio_ip                 *gpio;
        gpio_bridge             *gpio_loopback;
        // HMAC 
        hmac_ip                 *hmac;
        // CRNG
        crng_model              *csrng;
        // CLINT 
        CLINT<1>                *clint;
        // RISCV VeeR/El2
        VeeRISSTlm              *riscv; 
        // Bus 
        NetTrace                *debug_bus = NULL;
        SimpleBus<INIT_COUNT, TARG_COUNT> *bus;
        // PLIC 
        FE310_PLIC<1, PLIC_NUM_INTERRUPTS, 96, 32> *plic;
        // RESET Generation unit
        reset_generation_unit   *rsu_module;
        // KMAC 
        kmac_ip                 *kmac;
        // AES
	    aes_model               *aes;
        // SPI
        spi_flash               *spi_device;
        spi_controller_ip       *spi_controller;
        // OTBN interface stubs (must be declared before OTBN module for proper destruction order)
        edn_req_stub            *edn_rnd_req_stub_inst;
        edn_rsp_stub            *edn_rnd_rsp_stub_inst;
        edn_req_stub            *edn_urnd_req_stub_inst;
        otp_key_req_stub        *otp_key_req_stub_inst;
        otbn_ip                 *otbn;
        // EDN + KeyMgr TT + DRBG adapter
        keymgr_tt_model         *keymgr;
        mailbox_ip              *mailbox;
        mailbox_host_stub       *mbox_host;
        MailboxBridge           *mbox_bridge;
        aon_timer_ip            *aon_timer;
	    sep_efuse_model         *sep_efuse;
        lc_ctrl_model           *lc_ctrl;
        avbbus_ip               *avbbus;
	    entropy_src_ip          *entropy_src;
        edn_ip                  *edn;
        //XSPI
        xspi_dma_target_stub            *xspi_dma_stub;
        xspi_idle_initiator_stub        *xspi_axi_idle_ini;
        xspi_por_bootstrap_initiator    *xspi_por_idle_ini;
        xspi_ctrl_ip                    *xspi_ctrl;
        xspi_target_sc_module           *xspi_flash;

        sc_signal<bool, SC_MANY_WRITERS> reset_signal;
        sc_signal<bool, SC_MANY_WRITERS> uart_intr_signal;
        // SPI Controller signal declarations
        sc_signal<bool, SC_MANY_WRITERS> spi_clk_signal;
        sc_signal<bool, SC_MANY_WRITERS> spi_error_irq_signal;
        sc_signal<bool, SC_MANY_WRITERS> spi_event_irq_signal;
        sc_signal<bool, SC_MANY_WRITERS> spi_dma_trigger_signal;
        // New GPIO signals for single-pin module
        sc_signal<bool, SC_MANY_WRITERS> gpio_out_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_oe_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_in_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_interrupt_signal;
        // LSIO interface signals (tied off for now)
        sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_out_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_oe_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_in_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_access_signal;
        // PAD configuration signals
        sc_signal<sc_uint<3>> gpio_pad_drive_strength_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_pad_pull_enable_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_pad_pull_select_signal;
        sc_signal<bool, SC_MANY_WRITERS> gpio_pad_schmitt_enable_signal;
        // HMAC signals
        sc_signal<double, SC_MANY_WRITERS> hmac_clk_signal;
        sc_signal<bool, SC_MANY_WRITERS> hmac_done_signal;
        sc_signal<bool, SC_MANY_WRITERS> hmac_fifo_empty_signal;
        sc_signal<bool, SC_MANY_WRITERS> hmac_err_signal;
        sc_signal<bool, SC_MANY_WRITERS> hmac_alert_signal;
        // KMAC signal declarations
        sc_signal<bool, SC_MANY_WRITERS> kmac_clk_signal;
        sc_signal<bool, SC_MANY_WRITERS> kmac_lc_escalate_signal;
        sc_signal<bool, SC_MANY_WRITERS> kmac_idle_signal;
        sc_signal<bool, SC_MANY_WRITERS> kmac_intr_signal;
        // OTBN signal declarations
        sc_signal<double, SC_MANY_WRITERS> otbn_clk_core_signal;
        sc_signal<double, SC_MANY_WRITERS> otbn_clk_edn_signal;
        sc_signal<double, SC_MANY_WRITERS> otbn_clk_otp_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_intr_done_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_alert_fatal_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_alert_recov_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_lc_escalate_req_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_lc_escalate_rsp_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_lc_rma_req_signal;
        sc_signal<bool, SC_MANY_WRITERS> otbn_lc_rma_rsp_signal;
        // CRNG (CSRNG) Signal Declarations and Connections
        sc_signal<bool, SC_MANY_WRITERS> csrng_clk_signal;
        sc_signal<uint8_t> csrng_otp_en_signal;
        sc_signal<bool, SC_MANY_WRITERS> csrng_cs_cmd_req_done_signal;
        sc_signal<bool, SC_MANY_WRITERS> csrng_cs_entropy_req_signal;
        sc_signal<bool, SC_MANY_WRITERS> csrng_cs_hw_inst_exc_signal;
        sc_signal<bool, SC_MANY_WRITERS> csrng_cs_fatal_err_signal;
        sc_signal<bool, SC_MANY_WRITERS> csrng_recov_alert_signal;
        sc_signal<bool, SC_MANY_WRITERS> csrng_fatal_alert_signal;
        // AES Signal Declarations and Connections
        sc_signal<bool, SC_MANY_WRITERS> aes_clk_signal;
        sc_signal<bool, SC_MANY_WRITERS> aes_idle_signal;
        sc_signal<bool, SC_MANY_WRITERS> aes_lc_escalate_signal;
        sc_signal<bool, SC_MANY_WRITERS> aes_alert_recov_signal;
        sc_signal<bool, SC_MANY_WRITERS> aes_alert_fatal_signal;
        // DMA signals
        sc_signal<bool, SC_MANY_WRITERS> dma_done_intr_sig;
        sc_signal<bool, SC_MANY_WRITERS> dma_chunk_done_intr_sig;
        sc_signal<bool, SC_MANY_WRITERS> dma_error_intr_sig;
        sc_signal<sc_core::sc_time> dma_clk_signal;
        sc_signal<bool> dma_lsio_trigger[11];
        sc_signal<bool> dma_alert_fatal_sig;
        // Mailbox signals
        sc_signal<double, SC_MANY_WRITERS> mbox_clk_signal;
        sc_signal<bool, SC_MANY_WRITERS> mbox_irq0_signal;
        sc_signal<bool, SC_MANY_WRITERS> mbox_irq1_signal;
        //XSPI signals
        sc_signal<bool, SC_MANY_WRITERS> xspi_irq_signal;
        // AON Timer signals
        sc_signal<double, SC_MANY_WRITERS> aon_clk_aon_freq_signal;
        sc_signal<double, SC_MANY_WRITERS> aon_clk_sys_freq_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_sleep_mode_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_lc_escalate_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_intr_wkup_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_intr_bark_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_nmi_bark_signal;
        // Carries NMI handler address from stdout_device to VeeRISSTlm.
        // Set when firmware sends LOAD_NMI_ADDR (0x81) mailbox command.
        sc_signal<uint32_t> nmi_vec_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_wkup_req_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_rst_req_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_fatal_fault_signal;
        sc_signal<uint32_t> aon_racl_policies_signal;
        sc_signal<bool, SC_MANY_WRITERS> aon_racl_error_signal;
        // KeyMgr TT signals
        sc_signal<bool, SC_MANY_WRITERS>   keymgr_wipe_ni_signal;
        sc_signal<bool, SC_MANY_WRITERS>   keymgr_irq_signal;
        // Entropy Source signals
        sc_signal<bool, SC_MANY_WRITERS> entropy_src_irq_signal;
        // EDN signals
        sc_signal<double, SC_MANY_WRITERS> edn_clk_signal;
        sc_signal<bool, SC_MANY_WRITERS> edn_cmd_req_done_signal;
        sc_signal<bool, SC_MANY_WRITERS> edn_fatal_err_signal;
        sc_signal<bool, SC_MANY_WRITERS> edn_recov_alert_signal;
        sc_signal<bool, SC_MANY_WRITERS> edn_fatal_alert_signal;
        sc_signal<bool, SC_MANY_WRITERS> unused_irq_signal;
        std::vector<sc_signal<bool, SC_MANY_WRITERS>*> plic_inputs;

        // Load configuration file
        WdRiscv::HartConfig config;

        // Paramaters 
        ArgsCSML  *argsCSML;
        csml_param<uint64_t> globalQuantumNs;

        //  OCH SEP
        och_sep_ss(sc_module_name name, BasicOptions& opt_in)
            : sc_module(name)
            , opt(opt_in)
            , unused_irq_signal("unused_irq_signal")
            , plic_inputs(PLIC_NUM_INTERRUPTS, &unused_irq_signal)
            , globalQuantumNs("globalQuantumNs", 10)
            {
                argsCSML = new ArgsCSML(opt);
                Args &args = opt;

                if (not parseArgs(args))
                    return;
                if (args.help or args.version)
                    return;

                // Expand each target program string into program name and args.
                args.expandTargets();

                if (not args.configFile.empty()) {
                    std::filesystem::path configPath(args.configFile);
                    if (configPath.is_relative()) {
                        if (const char* baseDir = std::getenv("SEP_VP_INI_DIR"))
                            configPath = (std::filesystem::path(baseDir) / configPath).lexically_normal();
                        else
                            configPath = std::filesystem::absolute(configPath).lexically_normal();
                        args.configFile = configPath.string();
                    }
                    std::cout << "Veer ISS --configFile option set to:" << args.configFile << std::endl;
                    if (not config.loadConfigFile(args.configFile))
                        return;
                }

                tlm::tlm_global_quantum::instance().set(sc_core::sc_time(globalQuantumNs.get_param_value(), sc_core::SC_NS));

                // Create instances of the all the models in the ocb sep sybsystem
                sram            = new SEPMemory("sram", false);
                rom             = new SEPMemory("rom", true);
                itcm            = new SEPMemory("itcm", true);
                dtcm            = new SEPMemory("dtcm", false);
                stdout_dev      = new stdout_device("stdout");
                dma             = new dma_model("dma");
                dma_sys_adapter = new dma_sys_bus_adapter("dma_sys_adapter");
                uart            = new UART_with_terminal("uart", opt.uartTcpPort, false);
                gpio            = new gpio_ip("gpio");
                hmac            = new hmac_ip("hmac");
                kmac            = new kmac_ip("kmac");
                otbn            = new otbn_ip("otbn", 0x10000);
                csrng           = new crng_model("csrng");
                aes             = new aes_model("aes");
                mailbox         = new mailbox_ip("mailbox_ip");
                mbox_host       = new mailbox_host_stub("mailbox_host_stub");
                mbox_bridge     = new MailboxBridge("mailbox_bridge");
                aon_timer       = new aon_timer_ip("aon_timer");
                sep_efuse       = new sep_efuse_model("sep_efuse");
                lc_ctrl         = new lc_ctrl_model("lc_ctrl");
                avbbus          = new avbbus_ip("avbbus");
                entropy_src     = new entropy_src_ip("entropy_src");
                edn             = new edn_ip("edn");
                clint           = new CLINT<1>("clint");
                plic            = new FE310_PLIC<1, PLIC_NUM_INTERRUPTS, 96, 32>("plic");
                bus             = new SimpleBus<INIT_COUNT, TARG_COUNT>("bus", debug_bus, false);
                rsu_module      = new reset_generation_unit("rsu");
                gpio_loopback   = new gpio_bridge("gpio_loopback");
                // OTBN interface stubs (must be declared before OTBN module for proper destruction order)
                edn_rnd_req_stub_inst  = new edn_req_stub("edn_rnd_req_stub");
                edn_rnd_rsp_stub_inst  = new edn_rsp_stub("edn_rnd_rsp_stub");
                edn_urnd_req_stub_inst = new edn_req_stub("edn_urnd_req_stub");
                otp_key_req_stub_inst  = new otp_key_req_stub("otp_key_req_stub");
                // Note: edn_urnd_rsp and otp_key_rsp stubs not needed (sc_export bound internally)
                // EDN + KeyMgr TT + DRBG adapter

                keymgr       = new keymgr_tt_model("keymgr_tt");

                // Wire demotion-state callback from lc_ctrl to keymgr.
                // Registering the lambda here is safe — both modules are constructed.
                // The lambda executes lazily at KDF invocation time, reading live
                // DEMOTE_1/2 values written by BL1/BL2 firmware during boot.
                keymgr->set_demote_callback([this]() {
                    return lc_ctrl->get_demote_state();
                });

                spi_device     = new spi_flash("spi_flash");
                spi_controller = new spi_controller_ip("spi_controller");

                xspi_axi_idle_ini = new xspi_idle_initiator_stub("xspi_axi_idle_ini");
                xspi_por_idle_ini = new xspi_por_bootstrap_initiator("xspi_por_idle_ini");
                xspi_ctrl        = new xspi_ctrl_ip("xspi_ctrl", 0x3000, CSML_DEFAULT_VERBOSITY, 1);
                xspi_flash       = new xspi_target_sc_module("xspi_flash");
                xspi_dma_stub    = new xspi_dma_target_stub("xspi_dma_stub");

                gpio->set_prot_mode(0xFF);

                // Load ELF files.
                auto& target = args.expandedTargets;
                if (args.expandedTargets.empty())
                    throw std::runtime_error("No ELF file specified");

                auto elfFile = target.front().front();
                {
                    std::filesystem::path elfPath(elfFile);
                    if (elfPath.is_relative()) {
                        if (const char* baseDir = std::getenv("SEP_VP_INI_DIR"))
                            elfPath = (std::filesystem::path(baseDir) / elfPath).lexically_normal();
                        else
                            elfPath = std::filesystem::absolute(elfPath).lexically_normal();
                        elfFile = elfPath.string();
                        target.front().front() = elfFile;
                    }
                }
                if (args.verbose)
                    std::cerr << "Loading ELF file " << elfFile << '\n';

                if (elfFile.find("rom_sanity_test") != std::string::npos) {
                    // The ROM sanity test expects the boot ROM to be pre-initialized with a
                    // deterministic pattern (see rom_sanity_test.c). In the SystemC VP there
                    // is no external testbench to preload ROM contents, so do it here.
                    constexpr uint64_t ROM_PATTERN_MAGIC = 0xDEAD0000ULL;
                    constexpr uint32_t TEST_ENTRIES = 64;
                    std::vector<uint64_t> rom_init(TEST_ENTRIES);
                    for (uint32_t i = 0; i < TEST_ENTRIES; i++) {
                        rom_init[i] = (static_cast<uint64_t>(i) << 32) | ROM_PATTERN_MAGIC | (i & 0xFFFFU);
                    }
                    // Important: the bus converts global addresses (e.g. 0x1004_0000) to local
                    // offsets before calling into SEPMemory, so we must preload at offset 0.
                    rom->load_data(reinterpret_cast<const char*>(rom_init.data()), 0, rom_init.size() * sizeof(uint64_t));
                }
                ELFLoader loader(elfFile.c_str());
                size_t entry_point = loader.get_entrypoint();

                riscv = new VeeRISSTlm("rv31imc", args, config, entry_point);

                if (opt.entry_point.available)
                    entry_point = opt.entry_point.value;
                try {
                    loader.load_executable_image(*itcm, 0x20000, opt.itcm_start_addr);  // Load .text into ITCM NEW
                    loader.load_executable_image(*dtcm, 0x10000, opt.dtcm_start_addr);  // Load .data into DTCM NEW
                    loader.load_executable_image(*sram, opt.sram_size, opt.sram_start_addr);
                } catch (ELFLoader::load_executable_exception& e) {
                    std::cerr << e.what() << std::endl;
                    std::cerr << "Memory map: " << std::endl;
                    return;
                }

                unused_irq_signal.write(false);
                module_bind();
            }

        void module_bind() {
            rsu_module->rst_ni(reset_signal);
            rsu_module->reset_req_i(aon_rst_req_signal);
            stdout_dev->nmi_vec_o(nmi_vec_signal);
            // Address mapping
            {
                unsigned it = 0;
                bus->ports[it++] = new PortMapping(opt.sram_start_addr, opt.sram_end_addr, *sram);
                bus->ports[it++] = new PortMapping(opt.rom_start_addr, opt.rom_end_addr, *rom);
                bus->ports[it++] = new PortMapping(opt.plic_end_addr, opt.plic_end_addr, *plic);
                bus->ports[it++] = new PortMapping(opt.clint_start_addr, opt.clint_end_addr, *clint);
                bus->ports[it++] = new PortMapping(opt.dma_start_addr, opt.dma_end_addr, *dma);
                bus->ports[it++] = new PortMapping(opt.uart_start_addr, opt.uart_end_addr, *uart);
                bus->ports[it++] = new PortMapping(opt.gpio_start_addr, opt.gpio_end_addr, *gpio);
                bus->ports[it++] = new PortMapping(opt.hmac_start_addr, opt.hmac_end_addr, *hmac);
                bus->ports[it++] = new PortMapping(opt.otbn_start_addr, opt.otbn_end_addr, *otbn);
                bus->ports[it++] = new PortMapping(opt.itcm_start_addr, opt.itcm_end_addr, *itcm);
                bus->ports[it++] = new PortMapping(opt.dtcm_start_addr, opt.dtcm_end_addr, *dtcm);
                bus->ports[it++] = new PortMapping(opt.stdout_start_addr, opt.stdout_end_addr, *stdout_dev);
                bus->ports[it++] = new PortMapping(opt.spi_start_addr, opt.spi_end_addr, *spi_controller);
                bus->ports[it++] = new PortMapping(opt.kmac_start_addr, opt.kmac_end_addr, *kmac);
                bus->ports[it++] = new PortMapping(opt.csrng_start_addr, opt.csrng_end_addr, *csrng);
				bus->ports[it++] = new PortMapping(opt.aes_start_addr, opt.aes_end_addr, *aes);
                bus->ports[it++] = new PortMapping(opt.mbox_start_addr, opt.mbox_end_addr, *mbox_bridge);
                bus->ports[it++] = new PortMapping(opt.aon_timer_start_addr, opt.aon_timer_end_addr, *aon_timer);
                bus->ports[it++] = new PortMapping(opt.keymgr_mb_start_addr, opt.keymgr_mb_end_addr, *keymgr);
                bus->ports[it++] = new PortMapping(opt.keymgr_kpvlp_start_addr, opt.keymgr_kpvlp_end_addr, *keymgr);
				bus->ports[it++] = new PortMapping(opt.sep_efuse_start_addr, opt.sep_efuse_end_addr, *sep_efuse);
                bus->ports[it++] = new PortMapping(opt.lc_ctrl_start_addr, opt.lc_ctrl_end_addr, *lc_ctrl);
                bus->ports[it++] = new PortMapping(opt.xspi_start_addr, opt.xspi_end_addr, *xspi_ctrl);
                bus->ports[it++] = new PortMapping(opt.avbbus_start_addr, opt.avbbus_end_addr, *avbbus);
				bus->ports[it++] = new PortMapping(opt.entropy_src_start_addr, opt.entropy_src_end_addr, *entropy_src);
				bus->ports[it++] = new PortMapping(opt.edn_start_addr, opt.edn_end_addr, *edn);
            }	bus->mapping_complete();

            int unsigned it = 0;
            riscv->setMasterId(it);
            bus->tsocks[it++].bind(riscv->initiator_socket);
            bus->tsocks[it++].bind(dma->ot_initiator_socket);
            bus->tsocks[it++].bind(dma->ctn_initiator_socket);
            bus->tsocks[it++].bind(dma_sys_adapter->ini);
            dma->sys_initiator_socket.bind(dma_sys_adapter->tgt);

            {
                unsigned it = 0;
                bus->isocks[it++].bind(sram->tsock);
                bus->isocks[it++].bind(rom->tsock);
                bus->isocks[it++].bind(plic->tsock);
                bus->isocks[it++].bind(clint->tsock);
                bus->isocks[it++].bind(dma->target_socket);
                bus->isocks[it++].bind(uart->target_socket);
                bus->isocks[it++].bind(gpio->target_socket);
                bus->isocks[it++].bind(hmac->target_socket);
                bus->isocks[it++].bind(otbn->target_socket);
                bus->isocks[it++].bind(itcm->tsock);
                bus->isocks[it++].bind(dtcm->tsock);
                bus->isocks[it++].bind(stdout_dev->sock);
                bus->isocks[it++].bind(spi_controller->target_socket);
                bus->isocks[it++].bind(kmac->target_socket);
                bus->isocks[it++].bind(csrng->target_socket);
                bus->isocks[it++].bind(aes->target_socket);
                bus->isocks[it++].bind(mbox_bridge->tsock);
                bus->isocks[it++].bind(aon_timer->target_socket);
                bus->isocks[it++].bind(keymgr->mailbox_socket);
                bus->isocks[it++].bind(keymgr->kpvlp_socket);
                bus->isocks[it++].bind(sep_efuse->target_socket);
                bus->isocks[it++].bind(lc_ctrl->target_socket);
                bus->isocks[it++].bind(xspi_ctrl->target_socket);
                bus->isocks[it++].bind(avbbus->target_socket);
                bus->isocks[it++].bind(entropy_src->target_socket);
                bus->isocks[it++].bind(edn->target_socket);
            }

            // Stub UART reset and interrupt signal */
            uart->reset(reset_signal);
            uart->INTR(uart_intr_signal);

            // DMA signals
            dma->clk_i(dma_clk_signal);
            dma->rst_ni(reset_signal);
            dma_clk_signal.write(sc_core::sc_time(10, sc_core::SC_NS));
            dma->alert_fatal_fault(dma_alert_fatal_sig);
            dma_alert_fatal_sig.write(false);
            for (int i = 0; i < 11; i++) {
                dma_lsio_trigger[i].write(false);
                dma->lsio_trigger[i](dma_lsio_trigger[i]);
            }
            dma->dma_done_intr(dma_done_intr_sig);
            dma->dma_chunk_done_intr(dma_chunk_done_intr_sig);
            dma->dma_error_intr(dma_error_intr_sig);

            // Connect new GPIO signals
            // Reset signal
            gpio->rst_ni(reset_signal);
            // GPIO I/O signals
            gpio->gpio_out_o(gpio_out_signal);
            gpio->gpio_oe_o(gpio_oe_signal);
            gpio->gpio_in_i(gpio_in_signal);
            gpio->interrupt_o(gpio_interrupt_signal);
            // LSIO interface signals
            gpio->lsio_gpio_out_i(gpio_lsio_out_signal);
            gpio->lsio_gpio_oe_i(gpio_lsio_oe_signal);
            gpio->lsio_gpio_in_o(gpio_lsio_in_signal);
            gpio->lsio_access_i(gpio_lsio_access_signal);
            // PAD configuration signals
            gpio->pad_drive_strength_o(gpio_pad_drive_strength_signal);
            gpio->pad_pull_enable_o(gpio_pad_pull_enable_signal);
            gpio->pad_pull_select_o(gpio_pad_pull_select_signal);
            gpio->pad_schmitt_enable_o(gpio_pad_schmitt_enable_signal);
            // Initialize LSIO signals (tied off)
            gpio_lsio_out_signal.write(false);
            gpio_lsio_oe_signal.write(false);
            gpio_lsio_access_signal.write(false);
            // Create simple loopback: GPIO output connects back to GPIO input
            gpio_loopback->gpio_out_in(gpio_out_signal);
            gpio_loopback->gpio_in_out(gpio_in_signal);

            // Connect HMAC signals
            hmac->clk_i(hmac_clk_signal);
            hmac->rst_ni(reset_signal);
            hmac->intr_hmac_done(hmac_done_signal);
            hmac->intr_fifo_empty(hmac_fifo_empty_signal);
            hmac->intr_hmac_err(hmac_err_signal);
            hmac->alert_fatal_fault(hmac_alert_signal);
            // Initialize HMAC clock (50 MHz) and reset (active)
            hmac_clk_signal.write(50000000.0);  // 50 MHz in Hz
            keymgr->hmac_key_socket.bind(hmac->keymgr_tl_socket);

            // Connect KMAC signals
            kmac->clk_i(kmac_clk_signal);
            kmac->rst_ni(reset_signal);
            kmac->lc_escalate_en_i(kmac_lc_escalate_signal);
            kmac->idle_o(kmac_idle_signal);
            kmac->intr_o(kmac_intr_signal);
            // Bind KMAC keymgr key socket
            keymgr->kmac_key_socket.bind(kmac->keymgr_tl_socket);
            // Initialize KMAC signals
            kmac_clk_signal.write(true);
            kmac_lc_escalate_signal.write(false);  // No escalation
            
            // Initialize OTBN clock and reset signals
            otbn_clk_core_signal.write(100.0);  // 100 MHz
            otbn_clk_edn_signal.write(100.0);   // 100 MHz
            otbn_clk_otp_signal.write(100.0);   // 100 MHz
            // Connect OTBN clock and reset signals
            otbn->clk_core(otbn_clk_core_signal);
            otbn->clk_edn(otbn_clk_edn_signal);
            otbn->clk_otp(otbn_clk_otp_signal);
            otbn->rst_n(reset_signal);
            // Connect OTBN interrupt and alert signals
            otbn->intr_done(otbn_intr_done_signal);
            otbn->alert_fatal(otbn_alert_fatal_signal);
            otbn->alert_recov(otbn_alert_recov_signal);
            // Connect OTBN Life Cycle Controller signals
            otbn->lc_escalate_req(otbn_lc_escalate_req_signal);
            otbn->lc_escalate_rsp(otbn_lc_escalate_rsp_signal);
            otbn->lc_rma_req(otbn_lc_rma_req_signal);
            otbn->lc_rma_rsp(otbn_lc_rma_rsp_signal);
            // Bind OTBN EDN interface stubs (only sc_port, not sc_export)
            otbn->edn_rnd_req.bind(*edn_rnd_req_stub_inst);
            otbn->edn_rnd_rsp.bind(*edn_rnd_rsp_stub_inst);
            otbn->edn_urnd_req.bind(*edn_urnd_req_stub_inst);
            // Note: edn_urnd_rsp is sc_export - bound internally by OTBN model
            // Bind OTBN OTP interface stubs (only sc_port, not sc_export)
            otbn->otp_key_req.bind(*otp_key_req_stub_inst);
            // Note: otp_key_rsp is sc_export - bound internally by OTBN model
            // Bind OTBN keymgr key socket
            keymgr->otbn_key_socket.bind(otbn->keymgr_tl_socket);
            // Initialize OTBN signals
            otbn_clk_core_signal.write(50000000.0);    // 50 MHz
            otbn_clk_edn_signal.write(50000000.0);     // 50 MHz
            otbn_clk_otp_signal.write(50000000.0);     // 50 MHz
            otbn_lc_escalate_req_signal.write(false);  // No escalation
            otbn_lc_rma_req_signal.write(false);       // No RMA

            // Connect SPI signals
            spi_controller->clk_i(spi_clk_signal);
            spi_controller->rst_ni(reset_signal);
            spi_controller->error_irq(spi_error_irq_signal);
            spi_controller->spi_event_irq(spi_event_irq_signal);
            spi_controller->dma_trigger(spi_dma_trigger_signal);
            spi_device->rst_ni(reset_signal);
            spi_controller->spi_master(spi_device->spi_target);
            // Initialize SPI clock signal
            spi_clk_signal.write(true);

            // Bind mailbox internal sockets
            mbox_bridge->isock.bind(mailbox->socket0);
            mbox_host->isock.bind(mailbox->socket1);
            // Bind mailbox SystemC ports
            mailbox->clk_i(mbox_clk_signal);
            mailbox->rst_ni(reset_signal);
            mailbox->irq_o[0](mbox_irq0_signal);
            mailbox->irq_o[1](mbox_irq1_signal);
            mbox_clk_signal.write(50000000.0);   // 50 MHz

            // Connect AON Timer signals
            aon_timer->clk_aon_freq(aon_clk_aon_freq_signal);
            aon_timer->clk_sys_freq(aon_clk_sys_freq_signal);
            aon_timer->rst_n(reset_signal);
            aon_timer->rst_aon_n(reset_signal);
            aon_timer->sleep_mode(aon_sleep_mode_signal);
            aon_timer->lc_escalate_en(aon_lc_escalate_signal);
            aon_timer->intr_wkup_timer_expired(aon_intr_wkup_signal);
            aon_timer->intr_wdog_timer_bark(aon_intr_bark_signal);
            aon_timer->nmi_wdog_timer_bark(aon_nmi_bark_signal);
            aon_timer->wkup_req(aon_wkup_req_signal);
            aon_timer->aon_timer_rst_req(aon_rst_req_signal);
            aon_timer->fatal_fault(aon_fatal_fault_signal);
            aon_timer->racl_policies(aon_racl_policies_signal);
            aon_timer->racl_error(aon_racl_error_signal);
            aon_clk_aon_freq_signal.write(200000.0);   // 200 kHz AON clock
            aon_clk_sys_freq_signal.write(100000000.0); // 100 MHz sys clock
            aon_sleep_mode_signal.write(false);
            aon_lc_escalate_signal.write(false);
            aon_racl_policies_signal.write(0);

            // Connect Peripherals to PLIC
            plic_inputs[UART_IRQ]            = &uart_intr_signal;
            plic_inputs[SPI_ERROR_IRQ]       = &spi_error_irq_signal;
            plic_inputs[AON_WKUP_IRQ]        = &aon_intr_wkup_signal;
            plic_inputs[AON_WDOG_IRQ]        = &aon_intr_bark_signal;
            plic_inputs[XSPI_IRQ]            = &xspi_irq_signal;
            plic_inputs[GPIO_IRQ]            = &gpio_interrupt_signal;
            plic_inputs[KEYMGR_IRQ]          = &keymgr_irq_signal;
            plic_inputs[SPI_EVENT_IRQ]       = &spi_event_irq_signal;
            plic_inputs[HMAC_DONE_IRQ]       = &hmac_done_signal;
            plic_inputs[HMAC_FIFO_EMPTY_IRQ] = &hmac_fifo_empty_signal;
            plic_inputs[HMAC_HMAC_ERR_IRQ]   = &hmac_err_signal;
            plic_inputs[KMAC_IRQ]            = &kmac_intr_signal;
            plic_inputs[OTBN_IRQ]            = &otbn_intr_done_signal;
            plic_inputs[DMA_DONE_IRQ]        = &dma_done_intr_sig;
            plic_inputs[DMA_CHUNK_DONE_IRQ]  = &dma_chunk_done_intr_sig;
            plic_inputs[DMA_ERROR_IRQ]       = &dma_error_intr_sig;
            plic_inputs[MAILBOX_IRQ0]        = &mbox_irq0_signal;
            plic_inputs[MAILBOX_IRQ1]        = &mbox_irq1_signal;
            plic_inputs[CS_CMD_REQ_DONE]     = &csrng_cs_cmd_req_done_signal;
            plic_inputs[CS_ENTROPY_REQ]      = &csrng_cs_entropy_req_signal;
            plic_inputs[CS_FATAL_ERR]        = &csrng_cs_fatal_err_signal;
            plic_inputs[CS_HW_INST_EXC]      = &csrng_cs_hw_inst_exc_signal;
            plic_inputs[ENTROPY_SRC_IRQ]     = &entropy_src_irq_signal;
            plic_inputs[EDN_CMD_REQ_DONE]    = &edn_cmd_req_done_signal;
            plic_inputs[EDN_FATAL_ERR]       = &edn_fatal_err_signal;

            for (unsigned int i = 0; i < PLIC_NUM_INTERRUPTS; i++) {
                plic->inputs[i](*plic_inputs[i]);
            }

            // Initialize CRNG signals
            csrng_clk_signal.write(true);           // Clock active
            csrng_otp_en_signal.write(0x6);         // OTP enable = MuBi4True (enabled)

            // Connect CRNG clock and reset signals
            csrng->clk_i(csrng_clk_signal);
            csrng->rst_ni(reset_signal);
            csrng->otp_en_csrng_sw_app_read(csrng_otp_en_signal);

            // Connect CRNG interrupt signals
            csrng->cs_cmd_req_done(csrng_cs_cmd_req_done_signal);
            csrng->cs_entropy_req(csrng_cs_entropy_req_signal);
            csrng->cs_hw_inst_exc(csrng_cs_hw_inst_exc_signal);
            csrng->cs_fatal_err(csrng_cs_fatal_err_signal);
            // Connect CRNG alert signals
            csrng->recov_alert_o(csrng_recov_alert_signal);
            csrng->fatal_alert_o(csrng_fatal_alert_signal);

            // Connect AES signals
			aes->clk_i(aes_clk_signal);
			aes->rst_ni(reset_signal);
			aes->idle_o(aes_idle_signal);
			aes->lc_escalate_en(aes_lc_escalate_signal);
			aes->alert_recov_ctrl_update_err(aes_alert_recov_signal);
			aes->alert_fatal_fault(aes_alert_fatal_signal);
            // Bind AES interface stubs and keymgr key socket
			keymgr->aes_key_socket.bind(aes->keymgr_tl_socket);
            // Initialize AES signals
            aes_clk_signal.write(true);              // Clock active
            aes_lc_escalate_signal.write(false);     // No escalation

            // Entropy Source
			entropy_src->rst_ni(reset_signal);
			entropy_src->irq_o(entropy_src_irq_signal);

            // EDN
            edn_clk_signal.write(100.0);  // 100 MHz
            edn->clk_i(edn_clk_signal);
            edn->rst_ni(reset_signal);
            edn->intr_edn_cmd_req_done(edn_cmd_req_done_signal);
            edn->intr_edn_fatal_err(edn_fatal_err_signal);
            edn->alert_recov_alert(edn_recov_alert_signal);
            edn->alert_fatal_alert(edn_fatal_alert_signal);

            // KeyMgr TT
            keymgr_wipe_ni_signal.write(true);  // wipe inactive (active-low)
            keymgr->rst_ni(reset_signal);
            keymgr->wipe_ni(keymgr_wipe_ni_signal);
            keymgr->irq(keymgr_irq_signal);

            // XSPI controller + flash
            xspi_ctrl->reset_in(reset_signal);
            xspi_ctrl->int_out(xspi_irq_signal);
            xspi_por_idle_ini->rst_n(reset_signal);
            xspi_axi_idle_ini->socket.bind(xspi_ctrl->t_axi_slave_socket);
            xspi_por_idle_ini->socket.bind(xspi_ctrl->PoR_input_signals);
            xspi_ctrl->xspi_bus_socket[0]->bind(xspi_flash->target_socket);
            xspi_ctrl->i_dma_socket.bind(xspi_dma_stub->socket);

            riscv->rst_ni(reset_signal);
            riscv->nmi_i(aon_nmi_bark_signal);
            riscv->nmi_vec_i(nmi_vec_signal);
            plic->target_harts[0] = riscv;
            clint->target_harts[0] = riscv;
        }

        och_sep_ss(sc_module_name name)
            : och_sep_ss(name, och_sep_ss_default_options_ref()) {}

        void start_of_simulation() override {
            // Wire static OTP data from sep_efuse (fuse-burned: lc_state, chiplet_uid).
            // sep_efuse_model::end_of_elaboration() has already run load_fuses()
            // by this point, so m_lc_state_val is populated from config.
            keymgr_tt::km_firmware_handler::km_otp_data_t otp;
			otp.lc_state = sep_efuse->get_lc_state();
			std::copy(sep_efuse->get_chiplet_uid(),
			          sep_efuse->get_chiplet_uid() + 8,
                      otp.chiplet_uid);
            keymgr->set_otp_data(otp);
        }
};
