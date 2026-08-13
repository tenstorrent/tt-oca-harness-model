#include "VeeR-ISSTlm.hpp"
#include "HartConfig.hpp"
#include "Server.hpp"
#include <tlm.h>
#include <tlm_utils/tlm_quantumkeeper.h>
using namespace sc_core;

template <typename URV> unsigned notifyGdbAfterStop(WdRiscv::Hart<URV>& hart, int fd);

using namespace sc_core;
using namespace tlm;
using namespace WdRiscv;

void
Args::expandTargets()
{
	this->expandedTargets.clear();
	for (const auto& target : this->targets)
	{
		StringVec tokens;
		boost::split(tokens, target, boost::is_any_of(this->targetSep),
				boost::token_compress_on);
		this->expandedTargets.push_back(tokens);
	}
}


VeeRISSTlm::VeeRISSTlm(sc_module_name name, const Args &args, const WdRiscv::HartConfig &config, size_t entrypoint)
	: sc_module(name)
	, verbosity("verbosity", 2)
	, instrBatchSize("instrBatchSize", 100)
	, resetMemoryMappedRegister("resetMemoryMappedRegister", false)
	, enableNmi("enableNmi", false)
	, initiator_socket("initiator_socket")
	, rst_ni("rst_ni")
	, nmi_i("nmi_i")
	, nmi_vec_i("nmi_vec_i")
	, args_(args)
	, config_(config)
	, hart_id(0)
	, entrypoint(entrypoint)
	, pic_(sc_core::sc_module_name("pic"))
	, pic_isock_("pic_isock")
{
	pic_.bind_hart(this);
	pic_isock_.bind(pic_.target_socket);  // must bind in ctor, before elaboration port checks
	pic_.clk_i(pic_clk_);
	pic_.rst_ni(rst_ni);                  // PIC resets with the core it lives in
	logger.setMaxVerbosity(verbosity.get_param_value());
	logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
	logger.setFunctionTrace(false);
	CSML_INFO(2, logger) << "VeeRISSTlm module constructor" <<  std::endl;

	// Register SystemC Thread to single step though riscv
	SC_THREAD(run_thread);
	SC_METHOD(reset_method);
	sensitive << rst_ni;
	dont_initialize();

	// NMI handler: SC_THREAD waiting on nmi_i edges — naturally non-reentrant.
	// No dont_initialize(): thread starts at t=0, immediately blocks on
	// wait(nmi_i.posedge_event()) inside handle_nmi_signal, and stays there
	// until the AON bark signal fires.
	SC_THREAD(handle_nmi_signal);

	bool ok = session();
    if (ok) {
        CSML_INFO(2, logger) << "VeeRISSTlm module constructor completed" << std::endl;
    } else {
        CSML_INFO(2, logger) << "VeeRISSTlm module constructor error" << std::endl;
    }
}

VeeRISSTlm::~VeeRISSTlm() {}

void VeeRISSTlm::end_of_elaboration()
{
	// Register Callbacks to intercept memory accesses
	auto read_cb = [this](uint64_t addr, unsigned size, uint64_t &val) -> bool
	{
		return this->externalRead(addr, size, val);
	};
	auto write_cb = [this](uint64_t addr, unsigned size, uint64_t val) -> bool
	{
		return this->externalWrite(addr, size, val);
	};

	// Register Memory write and read callbacks
	system_->defineWriteMemoryCallback(write_cb);
	system_->defineReadMemoryCallback(read_cb);
	CSML_INFO(2, logger) << "VeeRISSTlm module end_of_elaboration - memory callback attached" <<  std::endl;

	// Debug prints
	auto hart0 = system_->ithHart(hart_id);
	// Debug prints 
	URV mstatus;
	hart0->peekCsr(CsrNumber::MSTATUS, mstatus);
	URV mie;
	hart0->peekCsr(CsrNumber::MIE, mie);
	URV mcause;
	hart0->peekCsr(CsrNumber::MCAUSE, mcause);
	URV mtval;
	hart0->peekCsr(CsrNumber::MTVAL, mtval);
	URV mepc;
	hart0->peekCsr(CsrNumber::MEPC, mepc);
	URV mtvec;
	hart0->peekCsr(CsrNumber::MTVEC, mtvec);

	CSML_DEBUG(3, logger) << "Register values before MIE enabled\n";
	CSML_DEBUG(3, logger) << "mstatus = 0x" << std::hex << mstatus << "\n";
	CSML_DEBUG(3, logger) << "mie     = 0x" << std::hex << mie << "\n";
	CSML_DEBUG(3, logger) << "mcause   = 0x" << std::hex << mcause << "\n";
	CSML_DEBUG(3, logger) << "mtval   = 0x" << std::hex << mtval << "\n";
	CSML_DEBUG(3, logger) << "mepc    = 0x" << std::hex << mepc << "\n";
	CSML_DEBUG(3, logger) << "mtvec   = 0x" << std::hex << mtvec << "\n";

	// Also enable global interrupts
	hart0->peekCsr(CsrNumber::MSTATUS, mstatus);	
	hart0->pokeCsr(CsrNumber::MSTATUS, mstatus | (1 << 3)); // MIE
	hart0->peekCsr(CsrNumber::MSTATUS, mstatus);
	hart0->peekCsr(CsrNumber::MIE, mie);
	hart0->peekCsr(CsrNumber::MCAUSE, mcause);
	hart0->peekCsr(CsrNumber::MTVAL, mtval);
	hart0->peekCsr(CsrNumber::MEPC, mepc);
	hart0->peekCsr(CsrNumber::MTVEC, mtvec);

	hart0->enableNmi(enableNmi.get_param_value());

	CSML_DEBUG(3, logger) << "Register values after MIE enabled\n";
	CSML_DEBUG(3, logger) << "mstatus = 0x" << std::hex << mstatus << "\n";
	CSML_DEBUG(3, logger) << "mie     = 0x" << std::hex << mie << "\n";
	CSML_DEBUG(3, logger) << "mcause  = 0x" << std::hex << mcause << "\n";
	CSML_DEBUG(3, logger) << "mtval   = 0x" << std::hex << mtval << "\n";
	CSML_DEBUG(3, logger) << "mepc    = 0x" << std::hex << mepc << "\n";
	CSML_DEBUG(3, logger) << "mtvec   = 0x" << std::hex << mtvec << "\n";
}

/// SystemC Veer/EL2 wrapper thread, which invokes the core
void VeeRISSTlm::run_thread()
{
	runThreadHandle_ = sc_core::sc_get_current_process_handle();

    while(1) {

        // Wait for all the systemc models to get reset
        if (!rst_ni.read()) 
        {
            wait(rst_ni.posedge_event());
        }
        try
        {
            auto &hart0 = *system_->ithHart(hart_id);

            bool result = sessionRun();

            if (not args_.instFreqFile.empty())
                result = reportInstructionFrequency(hart0, args_.instFreqFile) and result;

            closeUserFiles(args_, traceFile, commandLog, consoleOut, bblockFile);
        }
        catch (const sc_core::sc_unwind_exception&)
        {
            // SystemC uses sc_unwind_exception internally to implement process
            // kill/reset. It must not be swallowed.
            throw;
        }

        catch (std::exception &e)
        {
            std::cerr << e.what() << '\n';
        }
    }
}


void VeeRISSTlm::reset_method()
{
	const bool rst_n = rst_ni.read();

	if (not rst_n)
	{
		resetRequested_ = true;
		inReset_ = true;
		return;
	}

	// Rising edge: reset deasserted.
	if (inReset_)
	{
		do_reset_sequence();
		resetRequested_ = false;
		inReset_ = false;
#if 0
		if (runThreadHandle_.valid())
		{
			runThreadHandle_.reset();
			runThreadHandle_.resume();
		}
		#endif
	}
}


void VeeRISSTlm::do_reset_sequence()
{
	if (not system_)
		return;

	const bool resetMmioRegs = resetMemoryMappedRegister.get_param_value();

	for (unsigned i = 0; i < system_->hartCount(); ++i)
	{
		auto& hart = *system_->ithHart(i);
		hart.reset(resetMmioRegs);
		hart.pokePc(URV(entrypoint));

		// // Re-apply register initializations: hart.reset() clears all registers,
		// // but bare-metal firmware (e.g. bl1_pass_test) inherits SP from the
		// // caller, so regInits must survive reset just like they survive power-on.
		// for (const auto& regInit : args_.regInits) {
		// 	auto eq = regInit.find('=');
		// 	if (eq == std::string::npos) continue;
		// 	std::string regName = regInit.substr(0, eq);
		// 	const std::string regVal = regInit.substr(eq + 1);
		// 	auto colon = regName.find(':');
		// 	if (colon != std::string::npos) regName = regName.substr(colon + 1);
		// 	unsigned reg = 0;
		// 	if (hart.findIntReg(regName, reg)) {
		// 		URV val = 0;
		// 		try { val = URV(std::stoul(regVal, nullptr, 0)); } catch (...) {}
		// 		hart.pokeIntReg(reg, val);
		// 	}
		// }
	}
}

/// Depending on command line args, start a server, run in interactive
/// mode, or initiate a batch run.
bool VeeRISSTlm::sessionRun()
{
	bool waitAll = not args_.quitOnAnyHart;
	return batchRun(waitAll);
}

/// Open a server socket and put opened socket information (hostname
/// and port number) in the given server file. Wait for one
/// connection. Service connection. Return true on success and false
/// on failure.
bool VeeRISSTlm::runServer(const std::string& serverFile)
{
	char hostName[1024];
	if (gethostname(hostName, sizeof(hostName)) != 0)
	{
		std::cerr << "Failed to obtain name of this computer\n";
		return false;
	}

	int soc = socket(AF_INET, SOCK_STREAM, 0);
	if (soc < 0)
	{
		char buffer[512];
		char* p = buffer;
#ifdef __APPLE__
		strerror_r(errno, buffer, 512);
#else
		p = strerror_r(errno, buffer, 512);
#endif
		std::cerr << "Failed to create socket: " << p << '\n';
		return -1;
	}

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(0);

	if (bind(soc, (sockaddr*) &serverAddr, sizeof(serverAddr)) < 0)
	{
		perror("Socket bind failed");
		return false;
	}

	if (listen(soc, 1) < 0)
	{
		perror("Socket listen failed");
		return false;
	}

	sockaddr_in socAddr;
	socklen_t socAddrSize = sizeof(socAddr);
	socAddr.sin_family = AF_INET;
	socAddr.sin_port = 0;
	if (getsockname(soc, (sockaddr*) &socAddr,  &socAddrSize) == -1)
	{
		perror("Failed to obtain socket information");
		return false;
	}

	{
		std::ofstream out(serverFile);
		if (not out.good())
		{
			std::cerr << "Failed to open file '" << serverFile << "' for output\n";
			return false;
		}
		out << hostName << ' ' << ntohs(socAddr.sin_port) << std::endl;
	}

	sockaddr_in clientAddr;
	socklen_t clientAddrSize = sizeof(clientAddr);
	int newSoc = accept(soc, (sockaddr*) & clientAddr, &clientAddrSize);
	if (newSoc < 0)
	{
		perror("Socket accept failed");
		return false;
	}

	bool ok = true;

	try
	{
		Server<URV> server(*system_);
		ok = server.interact(newSoc, traceFile, commandLog);
	}
	catch(...)
	{
		ok = false;
	}

	close(newSoc);
	close(soc);

	return ok;
}

/// Run producing a snapshot after each snapPeriod instructions. Each
/// snapshot goes into its own directory names <dir><n> where <dir> is
/// the string in snapDir and <n> is a sequential integer starting at
/// 0. Return true on success and false on failure.
bool VeeRISSTlm::snapshotRun(const std::string& snapDir, uint64_t snapPeriod)
{
	if (not snapPeriod)
	{
		std::cerr << "Warning: Zero snap period ignored.\n";
		return batchRun(true /* waitAll */);
	}

	assert(system_->hartCount() == 1);
	Hart<URV>& hart = *(system_->ithHart(0));

	bool done = false;
	uint64_t globalLimit = hart.getInstructionCountLimit();

	while (not done)
	{
		uint64_t nextLimit = hart.getInstructionCount() +  snapPeriod;
		if (nextLimit >= globalLimit)
			done = true;
		nextLimit = std::min(nextLimit, globalLimit);
		hart.setInstructionCountLimit(nextLimit);
		hart.run(traceFile);
		while(hart.privilegeMode() != PrivilegeMode::User and not hart.hasTargetProgramFinished())
			hart.singleStep(traceFile);
		if (hart.hasTargetProgramFinished())
			done = true;
		if (not done)
		{
			unsigned index = hart.snapshotIndex();
			FileSystem::path path(snapDir + std::to_string(index));
			if (not FileSystem::is_directory(path))
				if (not FileSystem::create_directories(path))
				{
					std::cerr << "Error: Failed to create snapshot directory " << path << '\n';
					return false;
				}
			hart.setSnapshotIndex(index + 1);
			if (not hart.saveSnapshot(path))
			{
				std::cerr << "Error: Failed to save a snapshot\n";
				return false;
			}
		}
	}

#ifdef FAST_SLOPPY
	hart.reportOpenedFiles(std::cout);
#endif

	return true;
}


bool VeeRISSTlm::batchRun(bool waitAll)
{
	if (system_->hartCount() == 0)
		return true;

	// Re-evaluate PIC arbitration whenever firmware writes meipt or meicurpl.
	// RTL: mexintpend is combinational against those CSR values; our behavioral
	// model must re-check thresholds immediately after the CSRW completes.
	system_->ithHart(hart_id)->registerPostCsrInst(
		[this](unsigned, CsrNumber csrn) {
			uint32_t n = static_cast<uint32_t>(csrn);
			if (n == 0xBC9 || n == 0xBCC)
				pic_.notify_threshold_changed();
		});

	bool ok;
	if (system_->hartCount() == 1)
	{
		auto &hart = *system_->ithHart(0);
		unsigned int instrBatchSizeValue = instrBatchSize.get_param_value();
		InterruptCause cause;
		tlm_utils::tlm_quantumkeeper qk;
		URV cycle_count = 0;
		URV new_cycle_count = 0;

		while (1)
		{
			if (not rst_ni.read() or resetRequested_)
				return true;

			if (args_.gdb == false)
			{
				//Shorten the quantum after an interrupt becomes pending but still allow the step:
				int limit = hart.isInterruptPossible(cause) ? 1 : instrBatchSizeValue;

				for (int i = 0; i < limit; i++) {
					hart.singleStep(traceFile);
					// An interrupt/NMI may become pending mid-batch; stop early so we don't
					// overshoot serviceable interrupts at large instrBatchSize.
					if (hart.isInterruptPossible(cause))
						break;
				}

				hart.peekCsr(CsrNumber::MCYCLE, new_cycle_count);
				qk.inc(sc_time(10 * (new_cycle_count - cycle_count), SC_NS));  // or per-instruction estimate
				
				wait(qk.get_local_time(), rst_ni.negedge_event() | interruptWakeEvent_);  // Break on: quantum time elapse, rst_ni negedge (reset assert), or interruptWakeEvent_ (any interrupt asserted).
				qk.reset();

			        cycle_count = new_cycle_count;		

				if (not rst_ni.read())
					return true;
			} else
			{
				ok = hart.run(traceFile);
			}
		}
#ifdef FAST_SLOPPY
		hart.reportOpenedFiles(std::cout);
#endif
		return ok;
	}

	// Run each hart in its own thread.
	std::vector<std::thread> threadVec;

	std::atomic<bool> result = true;
	std::atomic<unsigned> finished = 0;  // Count of finished threads. 
	std::atomic<bool> hart0Done = false;

	FILE *tf = traceFile;

	auto threadFunc = [tf, &result, &finished, &hart0Done] (Hart<URV>* hart) {
		// In multi-hart system, wait till hart is started by hart0.
		while (not hart->isStarted())
			if (hart0Done)
				return;  // We are not going to be started.
		bool r = hart->run(tf);
		result = result and r;
		finished++;
		if (hart->sysHartIndex() == 0)
			hart0Done = true;
	};

	for (unsigned i = 0; i < system_->hartCount(); ++i)
	{
		Hart<URV>* hart = system_->ithHart(i).get();
		threadVec.emplace_back(std::thread(threadFunc, hart));
	}			      

	if (waitAll)
	{
		for (auto& t : threadVec)
			t.join();
	}
	else
	{
		// First thread to finish terminates run.
		while (finished == 0);

		extern void forceUserStop(int);
		forceUserStop(0);

		for (auto& t : threadVec)
			t.join();
	}

	return result;
}


bool VeeRISSTlm::session()
{

	if (not getPrimaryConfigParameters(args_, config_, hartsPerCore, coreCount,
				pageSize, memorySize, regionSize))
		return false;

	checkAndRepairMemoryParams(memorySize, pageSize, regionSize);

	// Create cores & harts.
	unsigned hartIdOffset = hartsPerCore;
	config_.getHartIdOffset(hartIdOffset);
	if (hartIdOffset < hartsPerCore)
	{
		std::cerr << "Invalid core_hart_id_offset: " << hartIdOffset
			<< ",  must be greater than harts_per_core: " << hartsPerCore << '\n';
		return false;
	}
	system_ = std::make_unique<System<URV> >(coreCount, hartsPerCore, hartIdOffset, memorySize, pageSize, regionSize);
	assert(system_->hartCount() == coreCount*hartsPerCore);
	assert(system_->hartCount() > 0);

	// Configure harts. Define callbacks for non-standard CSRs.
	bool userMode = args_.isa.find_first_of("uU") != std::string::npos;
	if (not config_.configHarts(*system_, userMode, args_.verbose))
		if (not args_.interactive)
			return false;

	// Configure memory.
	if (not config_.configMemory(*system_, args_.iccmRw, args_.unmappedElfOk, args_.verbose))
		return false;

	if (args_.hexFiles.empty() and args_.expandedTargets.empty()
			and not args_.interactive)
	{
		std::cerr << "No program file specified.\n";
		return false;
	}

	if (not openUserFiles(args_, traceFile, commandLog, consoleOut, bblockFile))
		return false;

	for (unsigned i = 0; i < system_->hartCount(); ++i)
	{
		auto& hart = *system_->ithHart(i);
		hart.setConsoleOutput(consoleOut);
		if (bblockFile)
			hart.enableBasicBlocks(bblockFile, args_.bblockInsts);
		hart.reset();
	}

	StringVec isaVec;
	if (not determineIsa(args_, isaVec))
		return false;

	URV pc;
	for (unsigned i = 0; i < system_->hartCount(); ++i)
	{
		if (not applyCmdLineArgs(args_, isaVec, *system_->ithHart(i), *system_))
			if (not args_.interactive)
				return false;

		(*system_->ithHart(i)).pokePc(URV(entrypoint));
		auto& hart = *system_->ithHart(i);
		pc = hart.peekPc();
		CSML_DEBUG(5, logger) << "PC value = 0x" << std::hex << pc << "\n";
	}

	// In server/interactive modes: enable triggers and performance counters.
	bool serverMode = not args_.serverFile.empty();
	if (serverMode or args_.interactive)
	{
		for (unsigned i = 0; i < system_->hartCount(); ++i)
		{
			auto &hart = *system_->ithHart(i);
			hart.enableTriggers(true);
			hart.enablePerformanceCounters(true);
		}
	}
	else
	{
		// Load error rollback is an annoyance if not in server/interactive mode
		for (unsigned i = 0; i < system_->hartCount(); ++i)
		{
			auto &hart = *system_->ithHart(i);
			hart.enableLoadErrorRollback(false);
			hart.enableBenchLoadExceptions(false);
		}
	}

	if (serverMode)
		return runServer(args_.serverFile);

	if (args_.interactive)
	{
		// Ignore keyboard interrupt for most commands. Long running
		// commands will enable keyboard interrupts while they run.
#ifdef __MINGW64__
		signal(SIGINT, kbdInterruptHandler);
#else
		struct sigaction newAction;
		sigemptyset(&newAction.sa_mask);
		newAction.sa_flags = 0;
		newAction.sa_handler = kbdInterruptHandler;
		sigaction(SIGINT, &newAction, nullptr);
#endif

		Interactive interactive(*system_);
		return interactive.interact(traceFile, commandLog);
	}

	if (args_.snapshotPeriod and *args_.snapshotPeriod)
	{
		uint64_t period = *args_.snapshotPeriod;
		std::string dir = args_.snapshotDir;
		if (system_->hartCount() == 1)
			return snapshotRun(dir, period);
		std::cerr << "Warning: Snapshots not supported for multi-thread runs\n";
	}

	return true;
}


bool VeeRISSTlm::reportInstructionFrequency(Hart<URV>& hart, const std::string& outPath)
{
	FILE* outFile = fopen(outPath.c_str(), "w");
	if (not outFile)
	{
		std::cerr << "Failed to open instruction frequency file '" << outPath
			<< "' for output.\n";
		return false;
	}
	hart.reportInstructionFrequency(outFile);
	hart.reportTrapStat(outFile);
	fprintf(outFile, "\n");
	hart.reportPmpStat(outFile);
	fprintf(outFile, "\n");
	hart.reportLrScStat(outFile);

	fclose(outFile);
	return true;
}

/// NMI handler — SC_THREAD waiting on bark (nmi_i) edges.
/// Mirrors VeeR RTL non-reentrant NMI: trigger once per bark assertion,
/// then absorb any AON-tick bounce before re-arming for the next NMI.
void VeeRISSTlm::handle_nmi_signal()
{
	while (true) {
		wait(nmi_i.posedge_event());
		interruptWakeEvent_.notify(sc_core::SC_ZERO_TIME);

		auto hart0 = system_->ithHart(hart_id);
		if (hart0->isNmiActive()) {
			CSML_INFO(2, logger) << "[NMI] bark fired while NMI active — ignoring\n";
			wait(nmi_i.negedge_event());
			continue;
		}

        uint32_t nmi_pc = nmi_vec_i.read();   // FW value (higher priority)

        if (nmi_pc != 0) {
            CSML_INFO(2, logger) << "[NMI] FW-provided nmi_vec = 0x"
                                 << std::hex << nmi_pc << "\n";
            hart0->defineNmiPc(nmi_pc);       // update core with FW value
        } else {
            nmi_pc = hart0->getNmiPc();       // fall back to JSON/core fix value
            if (nmi_pc == 0) {
                CSML_INFO(2, logger) << "[NMI] FW nmi_vec not set\n";
                CSML_INFO(2, logger) << "[NMI] core/json nmi_vec not set either\n";
                CSML_INFO(2, logger) << "[NMI] bark fired but nmi_vec not set — ignoring\n";
                continue;                     // skip, thread stays alive
            }
            CSML_INFO(2, logger) << "[NMI] FW nmi_vec not set, falling back to core/json value 0x"
                                 << std::hex << nmi_pc << "\n";
        }

        hart0->setPendingNmi(NmiCause::UNKNOWN);
        CSML_INFO(2, logger) << "[NMI] NMI triggered — hart will jump to 0x"
                             << std::hex << nmi_pc << "\n";
		
	}
}

/// External Interrupt interface function
void VeeRISSTlm::trigger_external_interrupt(PrivilegeLevel level)
{
	auto hart0 = system_->ithHart(hart_id);
	hart0->setExternalInterrupt(true);
	interruptWakeEvent_.notify(sc_core::SC_ZERO_TIME);
}

/// External Interrupt interface function
void VeeRISSTlm::clear_external_interrupt(PrivilegeLevel level)
{
	auto hart0 = system_->ithHart(hart_id);
	hart0->setExternalInterrupt(false);
}

/// VeeR EL2 fast-PIC: set the claim_id field [9:2] of MEIHAP. The upper
/// bits hold meivt's base; CsRegs::poke(MEIHAP, ...) preserves them and
/// only updates the claim_id field, so we just OR in (claim_id << 2).
void VeeRISSTlm::set_pic_claim_id(uint32_t claim_id)
{
	auto hart0 = system_->ithHart(hart_id);
	URV val = (URV(claim_id) & URV(0xFF)) << 2;
	hart0->pokeCsr(CsrNumber::MEIHAP, val);
}

bool VeeRISSTlm::peek_csr(uint32_t csr_num, uint32_t &val)
{
	auto hart0 = system_->ithHart(hart_id);
	URV v = 0;
	bool ok = hart0->peekCsr(CsrNumber(csr_num), v);
	val = static_cast<uint32_t>(v);
	return ok;
}

bool VeeRISSTlm::poke_csr(uint32_t csr_num, uint32_t val)
{
	auto hart0 = system_->ithHart(hart_id);
	return hart0->pokeCsr(CsrNumber(csr_num), URV(val));
}

/// notifyWrite used to invalidate reservations book keeping maintained by Hart,
/// in case of the reserved address written by another initiator
void VeeRISSTlm::notifyWrite(uint64_t addr, unsigned size, int initiator_id) 
{
	// Do not invalidate when the write comes fromVeeR2 itself
	if (this->initiator_id == initiator_id)
		return;

	auto hart0 = system_->ithHart(hart_id);
	hart0->externalInvalidateLr(addr, size);
} 

/// Direct TLM access to the internal PIC socket (bypasses system bus).
/// addr is the absolute system address; CSML memory expects a PIC-relative offset,
/// so we subtract PIC_BASE here (same translation the bus router would do).
bool VeeRISSTlm::doPicAccess(tlm_command cmd, uint64_t addr, unsigned char *data, unsigned len)
{
	tlm_generic_payload trans;
	trans.set_command(cmd);
	trans.set_address(addr - PIC_BASE);
	trans.set_data_ptr(data);
	trans.set_data_length(len);
	trans.set_streaming_width(len);
	trans.set_byte_enable_ptr(nullptr);
	trans.set_dmi_allowed(false);
	trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
	sc_time delay = sc_time(1, SC_NS);
	pic_isock_->b_transport(trans, delay);
	return trans.is_response_ok();
}

/// Read callback
bool VeeRISSTlm::externalRead(uint64_t addr, unsigned size, uint64_t &val)
{
	// This callback is invoked by Hart when it accesses memory.
	// We forward this to the TLM Initiator socket.
	uint8_t dataBuffer[8] = {0}; // Max 64-bit read
	val = 0;

	if (size > 8)
		return false;

	auto hart0 = system_->ithHart(hart_id);

	// PIC is internal to the VeeR EL2 core — route direct, not through system bus.
	if (addr >= PIC_BASE && addr < PIC_BASE + PIC_SIZE) {
		bool success = doPicAccess(TLM_READ_COMMAND, addr, dataBuffer, size);
		if (success) memcpy(&val, dataBuffer, size);
		return success;
	}

	bool success = doTlmAccess(TLM_READ_COMMAND, addr, dataBuffer, size,
			(hart0->inDebugMode() || hart0->gdbAccessInProgress()));

	if (success) {
		memcpy(&val, dataBuffer, size);
	} else if (!hart0->inDebugMode()) {
		// VeeR EL2: imprecise load bus error — post as NMI (MDSEAC+LOAD_EXCEPTION).
		if (!hart0->isNmiActive()) {
			uint32_t nmi_pc = nmi_vec_i.read();
			if (nmi_pc != 0)
				hart0->defineNmiPc(nmi_pc);
			hart0->pokeCsr(CsrNumber::MDSEAC, addr);
			hart0->setPendingNmi(NmiCause::LOAD_EXCEPTION);
			interruptWakeEvent_.notify(sc_core::SC_ZERO_TIME);
		}
		return true;   // suppress synchronous fault; NMI pending instead
	}

	CSML_DEBUG(3, logger) << "externalRead Done addr=0x" << hex << addr << " size=" << size << " val=" << val << " pc=" << hart0->peekPc() << std::endl;

	return success;
}

/// Write callback
bool VeeRISSTlm::externalWrite(uint64_t addr, unsigned size, uint64_t val)
{
	uint8_t dataBuffer[8] = {0}; // Max 64-bit read

	if (size > 8)
		return false;

	memcpy(dataBuffer, &val, size);

	auto hart0 = system_->ithHart(hart_id);

	// PIC is internal to the VeeR EL2 core — route direct, not through system bus.
	if (addr >= PIC_BASE && addr < PIC_BASE + PIC_SIZE) {
		return doPicAccess(TLM_WRITE_COMMAND, addr, dataBuffer, size);
	}

    if (addr >= 0x80000000 && addr < 0x80000010) {
        CSML_INFO(5, logger) << "STDOUT_DEVICE: write addr=0x" << hex << addr << "val=0x" << *dataBuffer << "size=%u" << size << "\n";
    }

	CSML_DEBUG(5, logger) << "externalWrite Done addr=0x" << hex << addr << " size=" << size << " val=" << val << " pc=" << hart0->peekPc() << std::endl;

	bool ok = doTlmAccess(TLM_WRITE_COMMAND, addr, dataBuffer, size,
			(hart0->inDebugMode() || hart0->gdbAccessInProgress()));
	if (!ok && !hart0->inDebugMode()) {
		// VeeR EL2: store bus errors are imprecise — post as NMI (MDSEAC+STORE_EXCEPTION)
		// rather than raising a synchronous STORE_ACC_FAULT. This matches RTL behaviour
		// where the write buffer absorbs the write and the error surfaces asynchronously.
		if (!hart0->isNmiActive()) {
			// Sync the NMI vector from the hardware register (set by firmware via
			// nmi_set_vector_reg()) before firing, otherwise the hart uses the JSON
			// default which may not match the firmware's _nmi_handler address.
			uint32_t nmi_pc = nmi_vec_i.read();
			if (nmi_pc != 0)
				hart0->defineNmiPc(nmi_pc);
			hart0->pokeCsr(CsrNumber::MDSEAC, addr);
			hart0->setPendingNmi(NmiCause::STORE_EXCEPTION);
			interruptWakeEvent_.notify(sc_core::SC_ZERO_TIME);
		}
		return true;   // suppress synchronous fault; NMI pending instead
	}
	return ok;
}

/// TLM Transport helper
bool VeeRISSTlm::doTlmAccess(tlm_command cmd, uint64_t addr, unsigned char *data, unsigned len, bool inDebugMode)
{

	tlm_generic_payload trans;
	trans.set_command(cmd);
	trans.set_address(addr);
	trans.set_data_ptr(data);
	trans.set_data_length(len);
	trans.set_streaming_width(len); // Normal access
	trans.set_byte_enable_ptr(0);	// No byte enables required for now
	trans.set_dmi_allowed(false);
	trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

	// Stamp the AXI sideband the outbound/inbound filters key off. Mirrors
	// sep_cpu.sv:450-460, which drives SEP_SOURCE_ID on every AW/AR/W user
	// field of the IFU, LSU and DBG ports.
	trans.set_extension(&axi_ext_);

	sc_time delay = sc_time(1, SC_NS);

	CSML_INFO(5, logger) << "VeeRISSTlm::doTlmAccess::Addr:0x" << addr << std::endl;
	CSML_INFO(5, logger) << "VeeRISSTlm::doTlmAccess::len:0x" << len << std::endl;
	CSML_INFO(5, logger) << "VeeRISSTlm: inDebugMode " << inDebugMode << std::endl;
	CSML_INFO(5, logger) << "VeeRISSTlm: doTlmAccess  end.." << std::endl;

	if (inDebugMode)
	{
		// Debug transport call
		initiator_socket->transport_dbg(trans);
	}
	else
	{
		// Blocking transport call
		initiator_socket->b_transport(trans, delay);
	}

	// Wait for the annotated delay (Loosely Timed)
	// if (delay != sc_time(0, SC_NS)) 
	// {
	//    wait(delay);
	//}

	// Detach before `trans` goes out of scope: the payload destructor frees the
	// extensions it still holds, and axi_ext_ is a member, not heap-allocated.
	trans.clear_extension<sep::sep_axi_extension>();

	return trans.is_response_ok();
}

