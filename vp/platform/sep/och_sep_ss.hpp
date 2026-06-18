#include <systemc.h>
#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <tlm_utils/simple_target_socket.h>
#include <tlm.h>
#include "sep_memory.h"
#include "secure_dma.h"
#include "uart_with_terminal.h"
#include "gpio.h"
#include "hmac.h"
#include "kmac.h"
#include "otbn.h"
#include "otbn_interfaces.h"
#include "spi_controller.h"
#include "spi_controller_interface.h"
#include "spi_flash.h"
#include "csrng.h"
#include "key_manager.h"
#include <openssl/rand.h>
#include <cstring>
#include "aes.h"
#include "mailbox.h"
#include "adapters.h"
#include "stubs.h"
#include "vp_devices.h"
#include "aon_timer.h"
#include "efuse.h"
#include "lifecycle_ctrl.h"
#include "AVBBus.h"
#include "entropy_src.h"
#include "edn.h"
#include "riscv_plic.h"
#include "clint.h"
#include "rv32/elf_loader.h"
#include "options.h"
#include "bus.h"
#include "HartConfig.hpp"
#include "VeeR-ISSTlm.hpp"
#include "Args.hpp"
#include "irq_map.h"

namespace {
    // Default options for och_sep_ss(sc_module_name) — delegates to two-arg ctor.
    inline BasicOptions& och_sep_ss_default_options_ref() {
        static BasicOptions o;
        return o;
    }
} // namespace

class och_sep_ss : public sc_module {

public:
    // VP bus topology — initiators: riscv, dma_ot, dma_ctn, dma_sys_adapter
    static constexpr unsigned int INIT_COUNT = 4;
    // Targets: sram, rom, plic, clint, dma, uart, gpio, hmac, otbn, itcm, dtcm,
    //          stdout, spi, kmac, csrng, aes, mailbox, aon_timer, keymgr_mb,
    //          keymgr_kpvlp, efuse, lc_ctrl, avbbus, entropy_src,
    //          edn,  (PLIC is internal to VeeRISSTlm; reset_ctrl removed)
    static constexpr unsigned int TARG_COUNT = 25;

    SC_HAS_PROCESS(och_sep_ss);

    och_sep_ss(sc_module_name name, BasicOptions& opt_in);
    och_sep_ss(sc_module_name name)
        : och_sep_ss(name, och_sep_ss_default_options_ref()) {}

    ~och_sep_ss() {
        // riscv first — ISS may have threads; stop before freeing other resources
        delete riscv;
        delete sram;
        delete rom;
        delete itcm;
        delete dtcm;
        delete stdout_dev;
        delete dma;
        delete dma_sys_adapter;
        delete uart;
        delete gpio;
        delete gpio_loopback;
        delete hmac;
        delete kmac;
        delete otbn;
        delete otp_key_req_stub_inst;
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
        delete spi_device;
        delete spi_controller;
        delete keymgr;
        delete rsu_module;
        delete clint;
        delete plic;
        for (auto* p : bus->ports)
            delete p;
        delete bus;
        delete argsCSML;
    }

private:
    BasicOptions opt;

    // =========================================================================
    // Peripheral models
    // =========================================================================
    SEPMemory*                        sram               = nullptr;
    SEPMemory*                        rom                = nullptr;
    SEPMemory*                        itcm               = nullptr;
    SEPMemory*                        dtcm               = nullptr;
    stdout_device*                    stdout_dev         = nullptr;
    secure_dma_model*                 dma                = nullptr;
    dma_sys_bus_adapter*              dma_sys_adapter    = nullptr;
    UART_with_terminal*               uart               = nullptr;
    gpio_ip*                          gpio               = nullptr;
    gpio_bridge*                      gpio_loopback      = nullptr;
    hmac_ip*                          hmac               = nullptr;
    kmac_ip*                          kmac               = nullptr;
    otbn_ip*                          otbn               = nullptr;
    otp_key_req_stub*                 otp_key_req_stub_inst = nullptr;
    csrng_model*                      csrng              = nullptr;
    aes_model*                        aes                = nullptr;
    mailbox_ip*                       mailbox            = nullptr;
    mailbox_host_stub*                mbox_host          = nullptr;
    MailboxBridge*                    mbox_bridge        = nullptr;
    aon_timer_ip*                     aon_timer          = nullptr;
    efuse_model*                      sep_efuse          = nullptr;
    lifecycle_ctrl_model*             lc_ctrl            = nullptr;
    avbbus_ip*                        avbbus             = nullptr;
    entropy_src_ip*                   entropy_src        = nullptr;
    edn_ip*                           edn                = nullptr;
    spi_flash*                        spi_device         = nullptr;
    spi_controller_ip*                spi_controller     = nullptr;
    key_manager_model*                keymgr             = nullptr;

    // =========================================================================
    // Platform infrastructure
    // =========================================================================
    CLINT<1>*                                  clint     = nullptr;
    VeeRISSTlm*                                riscv     = nullptr;  // created after ELF load
    SimpleBus<INIT_COUNT, TARG_COUNT>*         bus       = nullptr;
    FE310_PLIC<1, PLIC_NUM_INTERRUPTS, 96, 32>* plic    = nullptr;
    reset_generation_unit*                     rsu_module = nullptr;

    // =========================================================================
    // Signals — reset + per-IP clocks, interrupts, alerts
    // =========================================================================
    sc_signal<bool, SC_MANY_WRITERS> reset_signal;

    // UART
    sc_signal<bool, SC_MANY_WRITERS> uart_intr_signal;

    // SPI
    sc_signal<bool, SC_MANY_WRITERS> spi_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS> spi_error_irq_signal;
    sc_signal<bool, SC_MANY_WRITERS> spi_event_irq_signal;
    sc_signal<bool, SC_MANY_WRITERS> spi_dma_trigger_signal;

    // GPIO
    sc_signal<bool, SC_MANY_WRITERS> gpio_out_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_oe_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_in_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_interrupt_signal;
    // LSIO (tied off)
    sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_out_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_oe_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_in_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_lsio_access_signal;
    // PAD config
    sc_signal<sc_uint<3>>            gpio_pad_drive_strength_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_pad_pull_enable_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_pad_pull_select_signal;
    sc_signal<bool, SC_MANY_WRITERS> gpio_pad_schmitt_enable_signal;

    // HMAC
    sc_signal<double, SC_MANY_WRITERS> hmac_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_done_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_fifo_empty_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_err_signal;
    sc_signal<bool, SC_MANY_WRITERS>   hmac_alert_signal;

    // KMAC
    sc_signal<bool, SC_MANY_WRITERS> kmac_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS> kmac_lc_escalate_signal;
    sc_signal<bool, SC_MANY_WRITERS> kmac_idle_signal;
    sc_signal<bool, SC_MANY_WRITERS> kmac_intr_signal;

    // OTBN
    sc_signal<double, SC_MANY_WRITERS> otbn_clk_core_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_intr_done_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_alert_fatal_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_alert_recov_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_escalate_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_escalate_rsp_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_rma_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   otbn_lc_rma_rsp_signal;

    // CSRNG
    sc_signal<bool, SC_MANY_WRITERS> csrng_clk_signal;
    sc_signal<uint8_t>               csrng_otp_en_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_cmd_req_done_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_entropy_req_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_hw_inst_exc_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_cs_fatal_err_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_recov_alert_signal;
    sc_signal<bool, SC_MANY_WRITERS> csrng_fatal_alert_signal;

    // AES
    sc_signal<bool, SC_MANY_WRITERS> aes_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_idle_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_lc_escalate_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_alert_recov_signal;
    sc_signal<bool, SC_MANY_WRITERS> aes_alert_fatal_signal;

    // DMA
    sc_signal<bool, SC_MANY_WRITERS>   dma_done_intr_sig;
    sc_signal<bool, SC_MANY_WRITERS>   dma_chunk_done_intr_sig;
    sc_signal<bool, SC_MANY_WRITERS>   dma_error_intr_sig;
    sc_signal<sc_core::sc_time>        dma_clk_signal;
    sc_signal<bool>                    dma_lsio_trigger[11];
    sc_signal<bool>                    dma_alert_fatal_sig;

    // Mailbox
    sc_signal<double, SC_MANY_WRITERS> mbox_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS>   mbox_irq0_signal;
    sc_signal<bool, SC_MANY_WRITERS>   mbox_irq1_signal;

    // AON Timer
    sc_signal<double, SC_MANY_WRITERS> aon_clk_aon_freq_signal;
    sc_signal<double, SC_MANY_WRITERS> aon_clk_sys_freq_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_sleep_mode_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_lc_escalate_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_intr_wkup_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_intr_bark_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_nmi_bark_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_wkup_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_rst_req_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_fatal_fault_signal;
    sc_signal<uint32_t>                aon_racl_policies_signal;
    sc_signal<bool, SC_MANY_WRITERS>   aon_racl_error_signal;
    // NMI address from stdout_device to VeeRISSTlm (LOAD_NMI_ADDR command)
    sc_signal<uint32_t>                nmi_vec_signal;

    // KeyMgr
    sc_signal<bool, SC_MANY_WRITERS> keymgr_wipe_ni_signal;
    sc_signal<bool, SC_MANY_WRITERS> keymgr_irq_signal;

    // Entropy Source
    sc_signal<bool, SC_MANY_WRITERS> entropy_src_irq_signal;

    // EDN
    sc_signal<double, SC_MANY_WRITERS> edn_clk_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_cmd_req_done_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_fatal_err_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_recov_alert_signal;
    sc_signal<bool, SC_MANY_WRITERS>   edn_fatal_alert_signal;

    // PLIC input table — unused slots tied to unused_irq_signal
    sc_signal<bool, SC_MANY_WRITERS>              unused_irq_signal;
    std::vector<sc_signal<bool, SC_MANY_WRITERS>*> plic_inputs;

    // =========================================================================
    // Config / parameters
    // =========================================================================
    WdRiscv::HartConfig    config;
    ArgsCSML*              argsCSML      = nullptr;
    csml_param<uint64_t>   globalQuantumNs;

    // =========================================================================
    // Methods
    // =========================================================================
    void create_modules();
    void module_bind();
    void start_of_simulation() override;
};

// -----------------------------------------------------------------------------
// Constructor implementation
// -----------------------------------------------------------------------------
inline och_sep_ss::och_sep_ss(sc_module_name name, BasicOptions& opt_in)
    : sc_module(name)
    , opt(opt_in)
    , unused_irq_signal("unused_irq_signal")
    , plic_inputs(PLIC_NUM_INTERRUPTS, &unused_irq_signal)
    , globalQuantumNs("globalQuantumNs", 10)
{
    argsCSML = new ArgsCSML(opt);
    Args& args = opt;

    if (not parseArgs(args))
        return;
    if (args.help or args.version)
        return;

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

    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(static_cast<double>(globalQuantumNs.get_param_value()), sc_core::SC_NS));

    create_modules();

    // Load ELF — riscv is created after because it needs the entry point.
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
        // Pre-initialise ROM with a deterministic pattern expected by rom_sanity_test.c.
        // Bus strips base address before calling SEPMemory, so preload at offset 0.
        constexpr uint64_t ROM_PATTERN_MAGIC = 0xDEAD0000ULL;
        constexpr uint32_t TEST_ENTRIES = 64;
        std::vector<uint64_t> rom_init(TEST_ENTRIES);
        for (uint32_t i = 0; i < TEST_ENTRIES; i++)
            rom_init[i] = (static_cast<uint64_t>(i) << 32) | ROM_PATTERN_MAGIC | (i & 0xFFFFU);
        rom->load_data(reinterpret_cast<const char*>(rom_init.data()), 0, rom_init.size() * sizeof(uint64_t));
    }

    rv32::ELFLoader loader(elfFile.c_str());
    size_t entry_point = loader.get_entrypoint();

    riscv = new VeeRISSTlm("rv31imc", args, config, entry_point);

    if (opt.entry_point.available)
        entry_point = opt.entry_point.value;
    try {
        loader.load_executable_image(*itcm, 0x20000, opt.itcm_start_addr);
        loader.load_executable_image(*dtcm, 0x10000, opt.dtcm_start_addr);
        loader.load_executable_image(*sram, opt.sram_size, opt.sram_start_addr);
    } catch (rv32::ELFLoader::load_executable_exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Memory map: " << std::endl;
        return;
    }

    unused_irq_signal.write(false);
    module_bind();
}

// -----------------------------------------------------------------------------
// create_modules — instantiate all peripheral and infrastructure modules
// -----------------------------------------------------------------------------
inline void och_sep_ss::create_modules() {
    sram            = new SEPMemory("sram", false);
    rom             = new SEPMemory("rom", true);
    itcm            = new SEPMemory("itcm", false);
    dtcm            = new SEPMemory("dtcm", false);
    stdout_dev      = new stdout_device("stdout");
    dma             = new secure_dma_model("dma");
    dma_sys_adapter = new dma_sys_bus_adapter("dma_sys_adapter");
    uart            = new UART_with_terminal("uart", opt.uartTcpPort, false);
    gpio            = new gpio_ip("gpio");
    hmac            = new hmac_ip("hmac");
    kmac            = new kmac_ip("kmac");
    otbn            = new otbn_ip("otbn", 0x10000);
    // otp_key_rsp stub not needed — sc_export bound internally by OTBN model
    otp_key_req_stub_inst = new otp_key_req_stub("otp_key_req_stub");
    csrng           = new csrng_model("csrng");
    aes             = new aes_model("aes");
    mailbox         = new mailbox_ip("mailbox_ip");
    mbox_host       = new mailbox_host_stub("mailbox_host_stub");
    mbox_bridge     = new MailboxBridge("mailbox_bridge");
    aon_timer       = new aon_timer_ip("aon_timer");
    sep_efuse       = new efuse_model("sep_efuse");
    lc_ctrl         = new lifecycle_ctrl_model("lc_ctrl");
    avbbus          = new avbbus_ip("avbbus");
    entropy_src     = new entropy_src_ip("entropy_src");
    edn             = new edn_ip("edn");
    spi_device      = new spi_flash("spi_flash");
    spi_controller  = new spi_controller_ip("spi_controller");
    keymgr          = new key_manager_model("keymgr_tt");
    // Demotion-state callback reads live DEMOTE_1/2 values at KDF invocation time.
    keymgr->set_demote_callback([this]() { return lc_ctrl->get_demote_state(); });
    clint             = new CLINT<1>("CLINT");
    plic              = new FE310_PLIC<1, PLIC_NUM_INTERRUPTS, 96, 32>("plic");
    bus               = new SimpleBus<INIT_COUNT, TARG_COUNT>("bus", false);
    rsu_module        = new reset_generation_unit("rsu");
    gpio_loopback     = new gpio_bridge("gpio_loopback");
    gpio->set_prot_mode(0xFF);
}

// -----------------------------------------------------------------------------
// module_bind — wire all signals, sockets, and bus port mappings
// -----------------------------------------------------------------------------
inline void och_sep_ss::module_bind() {
    rsu_module->rst_ni(reset_signal);
    rsu_module->reset_req_i(aon_rst_req_signal);
    stdout_dev->nmi_vec_o(nmi_vec_signal);

    // Bus port mappings (address → target module)
    {
        unsigned it = 0;
        bus->ports[it++] = new PortMapping(opt.sram_start_addr,        opt.sram_end_addr,        *sram);
        bus->ports[it++] = new PortMapping(opt.rom_start_addr,         opt.rom_end_addr,         *rom);
        bus->ports[it++] = new PortMapping(opt.plic_start_addr,        opt.plic_end_addr,        *plic);
        bus->ports[it++] = new PortMapping(opt.clint_start_addr,       opt.clint_end_addr,       *clint);
        bus->ports[it++] = new PortMapping(opt.dma_start_addr,         opt.dma_end_addr,         *dma);
        bus->ports[it++] = new PortMapping(opt.uart_start_addr,        opt.uart_end_addr,        *uart);
        bus->ports[it++] = new PortMapping(opt.gpio_start_addr,        opt.gpio_end_addr,        *gpio);
        bus->ports[it++] = new PortMapping(opt.hmac_start_addr,        opt.hmac_end_addr,        *hmac);
        bus->ports[it++] = new PortMapping(opt.otbn_start_addr,        opt.otbn_end_addr,        *otbn);
        bus->ports[it++] = new PortMapping(opt.itcm_start_addr,        opt.itcm_end_addr,        *itcm);
        bus->ports[it++] = new PortMapping(opt.dtcm_start_addr,        opt.dtcm_end_addr,        *dtcm);
        bus->ports[it++] = new PortMapping(opt.stdout_start_addr,      opt.stdout_end_addr,      *stdout_dev);
        bus->ports[it++] = new PortMapping(opt.spi_start_addr,         opt.spi_end_addr,         *spi_controller);
        bus->ports[it++] = new PortMapping(opt.kmac_start_addr,        opt.kmac_end_addr,        *kmac);
        bus->ports[it++] = new PortMapping(opt.csrng_start_addr,       opt.csrng_end_addr,       *csrng);
        bus->ports[it++] = new PortMapping(opt.aes_start_addr,         opt.aes_end_addr,         *aes);
        bus->ports[it++] = new PortMapping(opt.mbox_start_addr,        opt.mbox_end_addr,        *mbox_bridge);
        bus->ports[it++] = new PortMapping(opt.aon_timer_start_addr,   opt.aon_timer_end_addr,   *aon_timer);
        bus->ports[it++] = new PortMapping(opt.keymgr_mb_start_addr,   opt.keymgr_mb_end_addr,   *keymgr);
        bus->ports[it++] = new PortMapping(opt.keymgr_kpvlp_start_addr,opt.keymgr_kpvlp_end_addr,*keymgr);
        bus->ports[it++] = new PortMapping(opt.sep_efuse_start_addr,   opt.sep_efuse_end_addr,   *sep_efuse);
        bus->ports[it++] = new PortMapping(opt.lc_ctrl_start_addr,     opt.lc_ctrl_end_addr,     *lc_ctrl);
        bus->ports[it++] = new PortMapping(opt.avbbus_start_addr,      opt.avbbus_end_addr,      *avbbus);
        bus->ports[it++] = new PortMapping(opt.entropy_src_start_addr, opt.entropy_src_end_addr, *entropy_src);
        bus->ports[it++] = new PortMapping(opt.edn_start_addr,         opt.edn_end_addr,         *edn);
    }
    bus->mapping_complete();

    // Initiator sockets → bus target sockets
    {
        unsigned it = 0;
        riscv->setMasterId(it);
        bus->tsocks[it++].bind(riscv->initiator_socket);
        bus->tsocks[it++].bind(dma->ot_initiator_socket);
        bus->tsocks[it++].bind(dma->ctn_initiator_socket);
        bus->tsocks[it++].bind(dma_sys_adapter->ini);
        dma->sys_initiator_socket.bind(dma_sys_adapter->tgt);
    }

    // Bus initiator sockets → target module sockets
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
        bus->isocks[it++].bind(avbbus->target_socket);
        bus->isocks[it++].bind(entropy_src->target_socket);
        bus->isocks[it++].bind(edn->target_socket);
    }

    // UART
    uart->reset(reset_signal);
    uart->INTR(uart_intr_signal);

    // DMA
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

    // GPIO
    gpio->rst_ni(reset_signal);
    gpio->gpio_out_o(gpio_out_signal);
    gpio->gpio_oe_o(gpio_oe_signal);
    gpio->gpio_in_i(gpio_in_signal);
    gpio->interrupt_o(gpio_interrupt_signal);
    gpio->lsio_gpio_out_i(gpio_lsio_out_signal);
    gpio->lsio_gpio_oe_i(gpio_lsio_oe_signal);
    gpio->lsio_gpio_in_o(gpio_lsio_in_signal);
    gpio->lsio_access_i(gpio_lsio_access_signal);
    gpio->pad_drive_strength_o(gpio_pad_drive_strength_signal);
    gpio->pad_pull_enable_o(gpio_pad_pull_enable_signal);
    gpio->pad_pull_select_o(gpio_pad_pull_select_signal);
    gpio->pad_schmitt_enable_o(gpio_pad_schmitt_enable_signal);
    gpio_lsio_out_signal.write(false);
    gpio_lsio_oe_signal.write(false);
    gpio_lsio_access_signal.write(false);
    gpio_loopback->gpio_out_in(gpio_out_signal);
    gpio_loopback->gpio_in_out(gpio_in_signal);

    // HMAC
    hmac->clk_i(hmac_clk_signal);
    hmac->rst_ni(reset_signal);
    hmac->intr_hmac_done(hmac_done_signal);
    hmac->intr_fifo_empty(hmac_fifo_empty_signal);
    hmac->intr_hmac_err(hmac_err_signal);
    hmac->alert_fatal_fault(hmac_alert_signal);
    hmac_clk_signal.write(50000000.0);  // 50 MHz
    keymgr->hmac_key_socket.bind(hmac->keymgr_tl_socket);

    // KMAC
    kmac->clk_i(kmac_clk_signal);
    kmac->rst_ni(reset_signal);
    kmac->lc_escalate_en_i(kmac_lc_escalate_signal);
    kmac->idle_o(kmac_idle_signal);
    kmac->intr_o(kmac_intr_signal);
    keymgr->kmac_key_socket.bind(kmac->keymgr_tl_socket);
    kmac_clk_signal.write(true);
    kmac_lc_escalate_signal.write(false);

    // OTBN
    otbn->clk_core(otbn_clk_core_signal);
    otbn->rst_n(reset_signal);
    otbn->intr_done(otbn_intr_done_signal);
    otbn->alert_fatal(otbn_alert_fatal_signal);
    otbn->alert_recov(otbn_alert_recov_signal);
    otbn->lc_escalate_req(otbn_lc_escalate_req_signal);
    otbn->lc_escalate_rsp(otbn_lc_escalate_rsp_signal);
    otbn->lc_rma_req(otbn_lc_rma_req_signal);
    otbn->lc_rma_rsp(otbn_lc_rma_rsp_signal);
    otbn->otp_key_req.bind(*otp_key_req_stub_inst);  // otp_key_rsp is sc_export, bound internally
    keymgr->otbn_key_socket.bind(otbn->keymgr_tl_socket);
    otbn_clk_core_signal.write(50000000.0);   // 50 MHz
    otbn_lc_escalate_req_signal.write(false);
    otbn_lc_rma_req_signal.write(false);

    // SPI
    spi_controller->clk_i(spi_clk_signal);
    spi_controller->rst_ni(reset_signal);
    spi_controller->error_irq(spi_error_irq_signal);
    spi_controller->spi_event_irq(spi_event_irq_signal);
    spi_controller->dma_trigger(dma_lsio_trigger[0]);  // SPI TX DMA handshake
    spi_device->rst_ni(reset_signal);
    spi_controller->spi_master(spi_device->spi_target);
    spi_clk_signal.write(true);

    // Mailbox
    mbox_bridge->isock.bind(mailbox->socket0);
    mbox_host->isock.bind(mailbox->socket1);
    mailbox->clk_i(mbox_clk_signal);
    mailbox->rst_ni(reset_signal);
    mailbox->irq_o[0](mbox_irq0_signal);
    mailbox->irq_o[1](mbox_irq1_signal);
    mbox_clk_signal.write(50000000.0);  // 50 MHz

    // AON Timer
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
    aon_clk_aon_freq_signal.write(200000.0);    // 200 kHz
    aon_clk_sys_freq_signal.write(100000000.0); // 100 MHz
    aon_sleep_mode_signal.write(false);
    aon_lc_escalate_signal.write(false);
    aon_racl_policies_signal.write(0);

    // PLIC interrupt routing
    plic_inputs[UART_IRQ]            = &uart_intr_signal;
    plic_inputs[SPI_ERROR_IRQ]       = &spi_error_irq_signal;
    plic_inputs[AON_WKUP_IRQ]        = &aon_intr_wkup_signal;
    plic_inputs[AON_WDOG_IRQ]        = &aon_intr_bark_signal;
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
    for (unsigned int i = 0; i < PLIC_NUM_INTERRUPTS; i++)
        plic->inputs[i](*plic_inputs[i]);

    // CSRNG
    csrng->clk_i(csrng_clk_signal);
    csrng->rst_ni(reset_signal);
    csrng->otp_en_csrng_sw_app_read(csrng_otp_en_signal);
    csrng->cs_cmd_req_done(csrng_cs_cmd_req_done_signal);
    csrng->cs_entropy_req(csrng_cs_entropy_req_signal);
    csrng->cs_hw_inst_exc(csrng_cs_hw_inst_exc_signal);
    csrng->cs_fatal_err(csrng_cs_fatal_err_signal);
    csrng->recov_alert_o(csrng_recov_alert_signal);
    csrng->fatal_alert_o(csrng_fatal_alert_signal);
    csrng_clk_signal.write(true);
    csrng_otp_en_signal.write(0x6);  // MuBi4True

    // AES
    aes->clk_i(aes_clk_signal);
    aes->rst_ni(reset_signal);
    aes->idle_o(aes_idle_signal);
    aes->lc_escalate_en(aes_lc_escalate_signal);
    aes->alert_recov_ctrl_update_err(aes_alert_recov_signal);
    aes->alert_fatal_fault(aes_alert_fatal_signal);
    keymgr->aes_key_socket.bind(aes->keymgr_tl_socket);
    aes_clk_signal.write(true);
    aes_lc_escalate_signal.write(false);

    // Entropy Source
    entropy_src->rst_ni(reset_signal);
    entropy_src->irq_o(entropy_src_irq_signal);

    // EDN
    edn->clk_i(edn_clk_signal);
    edn->rst_ni(reset_signal);
    edn->intr_edn_cmd_req_done(edn_cmd_req_done_signal);
    edn->intr_edn_fatal_err(edn_fatal_err_signal);
    edn->alert_recov_alert(edn_recov_alert_signal);
    edn->alert_fatal_alert(edn_fatal_alert_signal);
    edn_clk_signal.write(100.0);  // 100 MHz

    // KeyMgr
    keymgr->rst_ni(reset_signal);
    keymgr->wipe_ni(keymgr_wipe_ni_signal);
    keymgr->irq(keymgr_irq_signal);
    keymgr_wipe_ni_signal.write(true);  // active-low, inactive at startup

    // CPU core
    riscv->rst_ni(reset_signal);
    riscv->nmi_i(aon_nmi_bark_signal);
    riscv->nmi_vec_i(nmi_vec_signal);
    plic->target_harts[0]  = riscv;
    clint->target_harts[0] = riscv;
}

// -----------------------------------------------------------------------------
// start_of_simulation — wire OTP data from efuse into key manager
// -----------------------------------------------------------------------------
inline void och_sep_ss::start_of_simulation() {
    // efuse_model::end_of_elaboration() has already run load_fuses() by this
    // point, so lc_state and chiplet_uid are populated from the config file.
    keymgr_tt::km_firmware_handler::km_otp_data_t otp;
    otp.lc_state = sep_efuse->get_lc_state();
    std::copy(sep_efuse->get_chiplet_uid(),
              sep_efuse->get_chiplet_uid() + 8,
              otp.chiplet_uid);
    keymgr->set_otp_data(otp);
}
