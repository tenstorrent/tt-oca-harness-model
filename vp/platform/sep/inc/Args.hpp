#include "csml_parameter.h"
#include "VeeR-ISSTlm.hpp"

class BasicOptions : public Args {
    public:
        typedef uint32_t addr_t;

        addr_t el2_start_addr    = 0x00000000;
        addr_t el2_end_addr      = 0x0000FFFF;
        addr_t plic_start_addr   = 0x00200000;
        addr_t plic_end_addr     = 0x004FFFFF;  // covers hart_config at local +0x200000

        addr_t clint_start_addr  = 0x02000000;
        addr_t clint_end_addr    = 0x0200ffff;
        addr_t sys_start_addr    = 0x02010000;
        addr_t sys_end_addr      = 0x020103ff;
        addr_t rom_start_addr    = 0x10040000;
        addr_t rom_end_addr      = 0x1004FFFF;
        addr_t sram_start_addr   = 0x10000000;
        addr_t sram_end_addr     = 0x1003FFFF;
        addr_t dma_start_addr    = 0x10800000;
        addr_t dma_end_addr      = 0x1080014F;  // SECURE_DMA size 0x150
        addr_t uart_start_addr   = 0x44000000;  // VP placeholder (SMC window); confirm IO bridge offset
        addr_t uart_end_addr     = 0x4400FFFF;
        addr_t gpio_start_addr   = 0x46010000;  // VP placeholder (SMC window); confirm IO bridge offset
        addr_t gpio_end_addr     = 0x46010FFF;
        addr_t hmac_start_addr   = 0x10911000;
        addr_t hmac_end_addr     = 0x10912FFF;  // HMAC_REG size 0x2000
        addr_t otbn_start_addr   = 0x10900000;
        addr_t otbn_end_addr     = 0x1090BFFF;  // OTBN_REG size 0xC000
        addr_t stdout_start_addr = 0x80000000;
        addr_t stdout_end_addr   = 0x800000FF;

        // Aligned with silicon (och_sep_top_reg.h SEP_ICCM/SEP_DCCM):
        //   ICCM 0xC0000000..0xC003FFFF (256 KiB)
        //   DCCM 0xC0040000..0xC005FFFF (128 KiB)
        // This lets fw/sep/tests/ binaries run unchanged on VP and silicon.
        addr_t itcm_start_addr   = 0xC0000000;
        addr_t itcm_end_addr     = 0xC003FFFF;
        addr_t dtcm_start_addr   = 0xC0040000;
        addr_t dtcm_end_addr     = 0xC005FFFF;

        addr_t spi_start_addr    = 0x10B00000;  // SPI_CONTROLLER_REG base
        addr_t spi_end_addr      = 0x10B00037;  // SPI_CONTROLLER_REG size 0x38
        addr_t kmac_start_addr   = 0x10913000;
        addr_t kmac_end_addr     = 0x10913FFF;  // KMAC_REG size 0x1000
        addr_t aes_start_addr    = 0x10910000;
        addr_t aes_end_addr      = 0x10910087;  // AES_REG size 0x88
        addr_t csrng_start_addr  = 0x10915000;  // DRBG_CSRNG_BASE (sep_crypto_pkg.sv)
        addr_t csrng_end_addr    = 0x109157FF;  // DRBG_CSRNG size 0x800
        addr_t mbox_start_addr   = 0x10A00000;
        addr_t mbox_end_addr     = 0x10A0784F;  // AXIL_MAILBOX size 0x7850
        addr_t aon_timer_start_addr   = 0x10801000;  // WDT_TIMER_REG base
        addr_t aon_timer_end_addr     = 0x10801037;  // WDT_TIMER_REG size 0x38
        addr_t keymgr_mb_start_addr    = 0x10920000;
        addr_t keymgr_mb_end_addr      = 0x1092001B;  // KM_MAILBOX_SEP size 0x1C
        addr_t keymgr_kpvlp_start_addr = 0x10921000;  // KM_KPV_KPVLP base
        addr_t keymgr_kpvlp_end_addr   = 0x10921FFF;  // KM_KPV_KPVLP 4K block
        addr_t sep_efuse_start_addr    = 0x10930000;
        addr_t sep_efuse_end_addr      = 0x10930643;  // EFUSE_SHIM_CTRL end (0x10930600 + 0x44 - 1)
        addr_t lc_ctrl_start_addr      = 0x10918000;
        addr_t lc_ctrl_end_addr        = 0x10918017;  // SEP_LIFECYCLE_CTRL size 0x18
        addr_t avbbus_start_addr       = 0x46040000;  // VP placeholder (SMC window); confirm IO bridge offset
        addr_t avbbus_end_addr         = 0x4604005B;  // size 0x5C
        addr_t entropy_src_start_addr  = 0x10916000;  // ENTROPY_SOURCE_BASE (sep_crypto_pkg.sv)
        addr_t entropy_src_end_addr    = 0x10916FFF;  // ENTROPY_SOURCE size 0x1000
        addr_t edn_start_addr          = 0x10915800;  // DRBG_EDN_BASE (sep_crypto_pkg.sv)
        addr_t edn_end_addr            = 0x10915847;  // EDN register space (0x48 bytes)

        // Functional stubs (plain RW backing store, no behavioral model) — required by the
        // boot ROM early init. SEP_SCRATCH holds warm/cold scratch (warm reset detection);
        // OUTBOUND_FILTER_CTRL is programmed to allow mailbox egress. See och_sep_top_reg.h.
        addr_t sep_scratch_start_addr     = 0x10802000;  // SEP_SCRATCH cold/warm scratch
        addr_t sep_scratch_end_addr       = 0x10802FFF;  // 4 KiB
        addr_t outbound_filter_start_addr = 0x10A20000;  // OUTBOUND_FILTER_CTRL_0..N
        addr_t outbound_filter_end_addr   = 0x10A20FFF;  // 4 KiB

        // SMC global window: the sep_local_axi_xbar routes [0x40000000, 0xC0000000) to the
        // SMC via sep_system_peripherals (see fw/sep/bootcode sep_smc_interface.h). The boot
        // ROM coordinates with the SMC over this path (straps, CPU_CTRL scratch, chip-id,
        // fuse map, DFT status, and the 1 MiB SMC SRAM). The VP models nothing on the SMC
        // side, so these accesses faulted (store access fault). Model the low 2 MiB as a
        // functional RW stub: it covers all SMC registers (0x2090..0xF800) and SMC SRAM
        // (0x60000..0x15FFFF) with margin, and stays clear of both the avbbus placeholder
        // (0x46040000) and the SEP mailbox (0x80000000) — separate targets in the same
        // routed window.
        addr_t smc_global_start_addr      = 0x40000000;  // SEP_SMC_GLOBAL_BASE
        addr_t smc_global_end_addr        = 0x401FFFFF;  // 2 MiB (regs + SMC SRAM)

        int uartTcpPort = 8888;

        addr_t rom_size  = rom_end_addr - rom_start_addr + 1;
        addr_t sram_size = sram_end_addr - sram_start_addr + 1;

        bool quiet = false;
        OptionValue<uint64_t> entry_point;
};

// Note: All parameters of theVeer/EL2 are decalred as csml property for the sake of completness
// Not all pramaters can be used, as they might not be relevant in the OCH SEP platform
// Refer to the ini file  for the required parameters setting

class  ArgsCSML
{
    public:
    // Strings
    csml_param<std::string> traceFile;
    csml_param<std::string> commandLogFile;
    csml_param<std::string> consoleOutFile;
    csml_param<std::string> serverFile;
    csml_param<std::string> instFreqFile;
    csml_param<std::string> configFile;
    csml_param<std::string> bblockFile;
    csml_param<std::string> isa;
    csml_param<std::string> snapshotDir;
    csml_param<std::string> loadFrom;
    csml_param<std::string> stdoutFile;
    csml_param<std::string> stderrFile;
    csml_param<std::string> targetSep;

    csml_param<std::string> toHostSym;
    csml_param<bool>        has_toHostSym;

    csml_param<std::string> consoleIoSym;
    csml_param<bool>        has_consoleIoSym;

    // Vectors
    csml_param<std::vector<std::string>> hexFiles;
    csml_param<std::vector<std::string>> zisa;
    csml_param<std::vector<std::string>> regInits;
    csml_param<std::vector<std::string>> targets;
    csml_param<std::vector<unsigned>> gdbTcpPort;

    // Optional → represent via default + "has_*"
    csml_param<uint64_t> startPc;
    csml_param<bool>     has_startPc;

    csml_param<uint64_t> endPc;
    csml_param<bool>     has_endPc;

    csml_param<uint64_t> toHost;
    csml_param<bool>     has_toHost;

    csml_param<uint64_t> consoleIo;
    csml_param<bool>     has_consoleIo;

    csml_param<uint64_t> instCountLim;
    csml_param<bool>     has_instCountLim;

    csml_param<uint64_t> memorySize;
    csml_param<bool>     has_memorySize;

    csml_param<uint64_t> snapshotPeriod;
    csml_param<bool>     has_snapshotPeriod;

    csml_param<uint64_t> alarmInterval;
    csml_param<bool>     has_alarmInterval;

    csml_param<uint64_t> swInterrupt;
    csml_param<bool>     has_swInterrupt;

    csml_param<uint64_t> clint;
    csml_param<bool>     has_clint;

    csml_param<uint64_t> syscallSlam;
    csml_param<bool>     has_syscallSlam;

    // Scalars
    csml_param<unsigned> regWidth;
    csml_param<unsigned> harts;
    csml_param<unsigned> cores;
    csml_param<unsigned> pageSize;
    csml_param<uint64_t> bblockInsts;

    // Booleans
    csml_param<bool> help;
    csml_param<bool> trace;
    csml_param<bool> interactive;
    csml_param<bool> verbose;
    csml_param<bool> version;
    csml_param<bool> traceLdSt;
    csml_param<bool> csv;
    csml_param<bool> triggers;
    csml_param<bool> counters;
    csml_param<bool> gdb;
    csml_param<bool> abiNames;
    csml_param<bool> newlib;
    csml_param<bool> is_linux;
    csml_param<bool> raw;
    csml_param<bool> elfisa;
    csml_param<bool> fastExt;
    csml_param<bool> unmappedElfOk;
    csml_param<bool> iccmRw;
    csml_param<bool> quitOnAnyHart;
    csml_param<bool> noConInput;
    csml_param<bool> relativeInstCount;

    ArgsCSML(BasicOptions &a) 
    : traceFile("traceFile", a.traceFile)
    , commandLogFile("commandLogFile", a.commandLogFile)
    , consoleOutFile("consoleOutFile", a.consoleOutFile)
    , serverFile("serverFile", a.serverFile)
    , instFreqFile("instFreqFile", a.instFreqFile)
    , configFile("configFile", a.configFile)
    , bblockFile("bblockFile", a.bblockFile)
    , isa("isa", a.isa)
    , snapshotDir("snapshotDir", a.snapshotDir)
    , loadFrom("loadFrom", a.loadFrom)
    , stdoutFile("stdoutFile", a.stdoutFile)
    , stderrFile("stderrFile", a.stderrFile)
    , targetSep("targetSep", a.targetSep)

    , toHostSym("toHostSym", a.toHostSym.value_or(""))
    , has_toHostSym("has_toHostSym", a.toHostSym.has_value())

    , consoleIoSym("consoleIoSym", a.consoleIoSym.value_or(""))
    , has_consoleIoSym("has_consoleIoSym", a.consoleIoSym.has_value())

    , hexFiles("hexFiles", a.hexFiles)
    , zisa("zisa", a.zisa)
    , regInits("regInits", a.regInits)
    , targets("targets", a.targets)
    , gdbTcpPort("gdbTcpPort", a.gdbTcpPort)

    , startPc("startPc", a.startPc.value_or(0))
    , has_startPc("has_startPc", a.startPc.has_value())

    , endPc("endPc", a.endPc.value_or(0))
    , has_endPc("has_endPc", a.endPc.has_value())

    , toHost("toHost", a.toHost.value_or(0))
    , has_toHost("has_toHost", a.toHost.has_value())

    , consoleIo("consoleIo", a.consoleIo.value_or(0))
    , has_consoleIo("has_consoleIo", a.consoleIo.has_value())

    , instCountLim("instCountLim", a.instCountLim.value_or(0))
    , has_instCountLim("has_instCountLim", a.instCountLim.has_value())

    , memorySize("memorySize", a.memorySize.value_or(0))
    , has_memorySize("has_memorySize", a.memorySize.has_value())

    , snapshotPeriod("snapshotPeriod", a.snapshotPeriod.value_or(0))
    , has_snapshotPeriod("has_snapshotPeriod", a.snapshotPeriod.has_value())

    , alarmInterval("alarmInterval", a.alarmInterval.value_or(0))
    , has_alarmInterval("has_alarmInterval", a.alarmInterval.has_value())

    , swInterrupt("swInterrupt", a.swInterrupt.value_or(0))
    , has_swInterrupt("has_swInterrupt", a.swInterrupt.has_value())

    , clint("clint", a.clint.value_or(0))
    , has_clint("has_clint", a.clint.has_value())

    , syscallSlam("syscallSlam", a.syscallSlam.value_or(0))
    , has_syscallSlam("has_syscallSlam", a.syscallSlam.has_value())

    , regWidth("regWidth", a.regWidth)
    , harts("harts", a.harts)
    , cores("cores", a.cores)
    , pageSize("pageSize", a.pageSize)
    , bblockInsts("bblockInsts", a.bblockInsts)

    , help("help", a.help)
    , trace("trace", a.trace)
    , interactive("interactive", a.interactive)
    , verbose("verbose", a.verbose)
    , version("version", a.version)
    , traceLdSt("traceLdSt", a.traceLdSt)
    , csv("csv", a.csv)
    , triggers("triggers", a.triggers)
    , counters("counters", a.counters)
    , gdb("gdb", a.gdb)
    , abiNames("abiNames", a.abiNames)
    , newlib("newlib", a.newlib)
    , is_linux("is_linux", a.is_linux)
    , raw("raw", a.raw)
    , elfisa("elfisa", a.elfisa)
    , fastExt("fastExt", a.fastExt)
    , unmappedElfOk("unmappedElfOk", a.unmappedElfOk)
    , iccmRw("iccmRw", a.iccmRw)
    , quitOnAnyHart("quitOnAnyHart", a.quitOnAnyHart)
    , noConInput("noConInput", a.noConInput)
    , relativeInstCount("relativeInstCount", a.relativeInstCount)
    {
        const auto traceFile_v = traceFile.get_param_value();
        if (traceFile_v != a.traceFile) a.traceFile = traceFile_v;

        const auto commandLogFile_v = commandLogFile.get_param_value();
        if (commandLogFile_v != a.commandLogFile) a.commandLogFile = commandLogFile_v;

        const auto consoleOutFile_v = consoleOutFile.get_param_value();
        if (consoleOutFile_v != a.consoleOutFile) a.consoleOutFile = consoleOutFile_v;

        const auto serverFile_v = serverFile.get_param_value();
        if (serverFile_v != a.serverFile) a.serverFile = serverFile_v;

        const auto instFreqFile_v = instFreqFile.get_param_value();
        if (instFreqFile_v != a.instFreqFile) a.instFreqFile = instFreqFile_v;

        const auto configFile_v = configFile.get_param_value();
        if (configFile_v != a.configFile) a.configFile = configFile_v;

        const auto bblockFile_v = bblockFile.get_param_value();
        if (bblockFile_v != a.bblockFile) a.bblockFile = bblockFile_v;

        const auto isa_v = isa.get_param_value();
        if (isa_v != a.isa) a.isa = isa_v;

        const auto snapshotDir_v = snapshotDir.get_param_value();
        if (snapshotDir_v != a.snapshotDir) a.snapshotDir = snapshotDir_v;

        const auto loadFrom_v = loadFrom.get_param_value();
        if (loadFrom_v != a.loadFrom) a.loadFrom = loadFrom_v;

        const auto stdoutFile_v = stdoutFile.get_param_value();
        if (stdoutFile_v != a.stdoutFile) a.stdoutFile = stdoutFile_v;

        const auto stderrFile_v = stderrFile.get_param_value();
        if (stderrFile_v != a.stderrFile) a.stderrFile = stderrFile_v;

        const auto targetSep_v = targetSep.get_param_value();
        if (targetSep_v != a.targetSep) a.targetSep = targetSep_v;

        const bool has_toHostSym_v = has_toHostSym.get_param_value();
        if (has_toHostSym_v != a.toHostSym.has_value()) {
            if (has_toHostSym_v) a.toHostSym = toHostSym.get_param_value();
            else a.toHostSym.reset();
        } else if (has_toHostSym_v) {
            const auto toHostSym_v = toHostSym.get_param_value();
            if (toHostSym_v != a.toHostSym.value()) a.toHostSym = toHostSym_v;
        }

        const bool has_consoleIoSym_v = has_consoleIoSym.get_param_value();
        if (has_consoleIoSym_v != a.consoleIoSym.has_value()) {
            if (has_consoleIoSym_v) a.consoleIoSym = consoleIoSym.get_param_value();
            else a.consoleIoSym.reset();
        } else if (has_consoleIoSym_v) {
            const auto consoleIoSym_v = consoleIoSym.get_param_value();
            if (consoleIoSym_v != a.consoleIoSym.value()) a.consoleIoSym = consoleIoSym_v;
        }

        const auto hexFiles_v = hexFiles.get_param_value();
        if (hexFiles_v != a.hexFiles) a.hexFiles = hexFiles_v;

        const auto zisa_v = zisa.get_param_value();
        if (zisa_v != a.zisa) a.zisa = zisa_v;

        const auto regInits_v = regInits.get_param_value();
        if (regInits_v != a.regInits) a.regInits = regInits_v;

        const auto targets_v = targets.get_param_value();
        if (targets_v != a.targets) a.targets = targets_v;

        std::cout << "CCI param name for Elf: ";
        for (size_t i = 0; i < a.targets.size(); ++i) {
            std::cout << "targets[" << i << "] " << a.targets[i] << "\n";
        }

        const auto gdbTcpPort_v = gdbTcpPort.get_param_value();
        if (gdbTcpPort_v != a.gdbTcpPort) a.gdbTcpPort = gdbTcpPort_v;

        const bool has_startPc_v = has_startPc.get_param_value();
        if (has_startPc_v != a.startPc.has_value()) {
            if (has_startPc_v) a.startPc = startPc.get_param_value();
            else a.startPc.reset();
        } else if (has_startPc_v) {
            const auto startPc_v = startPc.get_param_value();
            if (startPc_v != a.startPc.value()) a.startPc = startPc_v;
        }

        const bool has_endPc_v = has_endPc.get_param_value();
        if (has_endPc_v != a.endPc.has_value()) {
            if (has_endPc_v) a.endPc = endPc.get_param_value();
            else a.endPc.reset();
        } else if (has_endPc_v) {
            const auto endPc_v = endPc.get_param_value();
            if (endPc_v != a.endPc.value()) a.endPc = endPc_v;
        }

        const bool has_toHost_v = has_toHost.get_param_value();
        if (has_toHost_v != a.toHost.has_value()) {
            if (has_toHost_v) a.toHost = toHost.get_param_value();
            else a.toHost.reset();
        } else if (has_toHost_v) {
            const auto toHost_v = toHost.get_param_value();
            if (toHost_v != a.toHost.value()) a.toHost = toHost_v;
        }

        const bool has_consoleIo_v = has_consoleIo.get_param_value();
        if (has_consoleIo_v != a.consoleIo.has_value()) {
            if (has_consoleIo_v) a.consoleIo = consoleIo.get_param_value();
            else a.consoleIo.reset();
        } else if (has_consoleIo_v) {
            const auto consoleIo_v = consoleIo.get_param_value();
            if (consoleIo_v != a.consoleIo.value()) a.consoleIo = consoleIo_v;
        }

        const bool has_instCountLim_v = has_instCountLim.get_param_value();
        if (has_instCountLim_v != a.instCountLim.has_value()) {
            if (has_instCountLim_v) a.instCountLim = instCountLim.get_param_value();
            else a.instCountLim.reset();
        } else if (has_instCountLim_v) {
            const auto instCountLim_v = instCountLim.get_param_value();
            if (instCountLim_v != a.instCountLim.value()) a.instCountLim = instCountLim_v;
        }

        const bool has_memorySize_v = has_memorySize.get_param_value();
        if (has_memorySize_v != a.memorySize.has_value()) {
            if (has_memorySize_v) a.memorySize = memorySize.get_param_value();
            else a.memorySize.reset();
        } else if (has_memorySize_v) {
            const auto memorySize_v = memorySize.get_param_value();
            if (memorySize_v != a.memorySize.value()) a.memorySize = memorySize_v;
        }

        const bool has_snapshotPeriod_v = has_snapshotPeriod.get_param_value();
        if (has_snapshotPeriod_v != a.snapshotPeriod.has_value()) {
            if (has_snapshotPeriod_v) a.snapshotPeriod = snapshotPeriod.get_param_value();
            else a.snapshotPeriod.reset();
        } else if (has_snapshotPeriod_v) {
            const auto snapshotPeriod_v = snapshotPeriod.get_param_value();
            if (snapshotPeriod_v != a.snapshotPeriod.value()) a.snapshotPeriod = snapshotPeriod_v;
        }

        const bool has_alarmInterval_v = has_alarmInterval.get_param_value();
        if (has_alarmInterval_v != a.alarmInterval.has_value()) {
            if (has_alarmInterval_v) a.alarmInterval = alarmInterval.get_param_value();
            else a.alarmInterval.reset();
        } else if (has_alarmInterval_v) {
            const auto alarmInterval_v = alarmInterval.get_param_value();
            if (alarmInterval_v != a.alarmInterval.value()) a.alarmInterval = alarmInterval_v;
        }

        const bool has_swInterrupt_v = has_swInterrupt.get_param_value();
        if (has_swInterrupt_v != a.swInterrupt.has_value()) {
            if (has_swInterrupt_v) a.swInterrupt = swInterrupt.get_param_value();
            else a.swInterrupt.reset();
        } else if (has_swInterrupt_v) {
            const auto swInterrupt_v = swInterrupt.get_param_value();
            if (swInterrupt_v != a.swInterrupt.value()) a.swInterrupt = swInterrupt_v;
        }

        const bool has_clint_v = has_clint.get_param_value();
        if (has_clint_v != a.clint.has_value()) {
            if (has_clint_v) a.clint = clint.get_param_value();
            else a.clint.reset();
        } else if (has_clint_v) {
            const auto clint_v = clint.get_param_value();
            if (clint_v != a.clint.value()) a.clint = clint_v;
        }

        const bool has_syscallSlam_v = has_syscallSlam.get_param_value();
        if (has_syscallSlam_v != a.syscallSlam.has_value()) {
            if (has_syscallSlam_v) a.syscallSlam = syscallSlam.get_param_value();
            else a.syscallSlam.reset();
        } else if (has_syscallSlam_v) {
            const auto syscallSlam_v = syscallSlam.get_param_value();
            if (syscallSlam_v != a.syscallSlam.value()) a.syscallSlam = syscallSlam_v;
        }

        const auto regWidth_v = regWidth.get_param_value();
        if (regWidth_v != a.regWidth) a.regWidth = regWidth_v;

        const auto harts_v = harts.get_param_value();
        if (harts_v != a.harts) a.harts = harts_v;

        const auto cores_v = cores.get_param_value();
        if (cores_v != a.cores) a.cores = cores_v;

        const auto pageSize_v = pageSize.get_param_value();
        if (pageSize_v != a.pageSize) a.pageSize = pageSize_v;

        const auto bblockInsts_v = bblockInsts.get_param_value();
        if (bblockInsts_v != a.bblockInsts) a.bblockInsts = bblockInsts_v;

        const bool help_v = help.get_param_value();
        if (help_v != a.help) a.help = help_v;

        const bool trace_v = trace.get_param_value();
        if (trace_v != a.trace) a.trace = trace_v;

        const bool interactive_v = interactive.get_param_value();
        if (interactive_v != a.interactive) a.interactive = interactive_v;

        const bool verbose_v = verbose.get_param_value();
        if (verbose_v != a.verbose) a.verbose = verbose_v;

        const bool version_v = version.get_param_value();
        if (version_v != a.version) a.version = version_v;

        const bool traceLdSt_v = traceLdSt.get_param_value();
        if (traceLdSt_v != a.traceLdSt) a.traceLdSt = traceLdSt_v;

        const bool csv_v = csv.get_param_value();
        if (csv_v != a.csv) a.csv = csv_v;

        const bool triggers_v = triggers.get_param_value();
        if (triggers_v != a.triggers) a.triggers = triggers_v;

        const bool counters_v = counters.get_param_value();
        if (counters_v != a.counters) a.counters = counters_v;

        const bool gdb_v = gdb.get_param_value();
        if (gdb_v != a.gdb) a.gdb = gdb_v;

        const bool abiNames_v = abiNames.get_param_value();
        if (abiNames_v != a.abiNames) a.abiNames = abiNames_v;

        const bool newlib_v = newlib.get_param_value();
        if (newlib_v != a.newlib) a.newlib = newlib_v;

        const bool is_linux_v = is_linux.get_param_value();
        if (is_linux_v != a.is_linux) a.is_linux = is_linux_v;

        const bool raw_v = raw.get_param_value();
        if (raw_v != a.raw) a.raw = raw_v;

        const bool elfisa_v = elfisa.get_param_value();
        if (elfisa_v != a.elfisa) a.elfisa = elfisa_v;

        const bool fastExt_v = fastExt.get_param_value();
        if (fastExt_v != a.fastExt) a.fastExt = fastExt_v;

        const bool unmappedElfOk_v = unmappedElfOk.get_param_value();
        if (unmappedElfOk_v != a.unmappedElfOk) a.unmappedElfOk = unmappedElfOk_v;

        const bool iccmRw_v = iccmRw.get_param_value();
        if (iccmRw_v != a.iccmRw) a.iccmRw = iccmRw_v;

        const bool quitOnAnyHart_v = quitOnAnyHart.get_param_value();
        if (quitOnAnyHart_v != a.quitOnAnyHart) a.quitOnAnyHart = quitOnAnyHart_v;

        const bool noConInput_v = noConInput.get_param_value();
        if (noConInput_v != a.noConInput) a.noConInput = noConInput_v;

        const bool relativeInstCount_v = relativeInstCount.get_param_value();
        if (relativeInstCount_v != a.relativeInstCount) a.relativeInstCount = relativeInstCount_v;

        std::cout << "Veer ISS configFile: " << a.configFile << std::endl;;
        std::cout << "Veer ISS gdb enabled: " << a.gdb << std::endl;
    }
    
};
