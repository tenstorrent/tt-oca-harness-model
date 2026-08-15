#ifndef RISCV_ISA_BUS_H
#define RISCV_ISA_BUS_H

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <systemc>

#include "initator_ext.h"
#include "csml_logger.h"
#include "csml_parameter.h"

/**************************************************************************/
/*** Problem statement ***/
/**************************************************************************
//  Current Design 
1. VeeR-ISS implements LR/SC internally
(it has makeLr(), invalidateLr(), reservation tracking)
2. But actual memory accesses go through external memory callbacks
3. That external memory sits behind a SystemC bus
4. DMA is another master on that bus

RISC-V does not use bus lock for LR/SC It uses reservation tracking in memory system / coherence fabric. 
So bus locking(which is implemneted here in this file) would not fix coherence issue, using the reservation 
logic built in VeeR2 would resolvei the coherence issue.
If any agent(Another hart, DMA, Debug port, Any bus maste)writes to the reserved address,
the reservation must be cleared. 

// Changes to the design 
1. VeeR2 keeps its reservation tracking in Memory(internal to VeeR2)
2. External SystemC memory model does NOT implement reservation logic
3. The SystemC bus snoops writes and notifies interested parties (like VeeR2)
4. Only writes from other initiators (e.g., DMA) invalidate reservations
6. VeeR2 monitors the invalidations
7. No tight coupling between memory and DMA
8. Implement SetMasterId for all initiators and this should be called from sc_main, after the port connections are done.
9. Implement externalInvalidateLr in Hart. This will be called from Initiator's notifyWrite function.
    void externalInvalidateLr(uint64_t addr, unsigned size)
    {
      std::lock_guard<std::mutex> lock(memory_.lrMutex_);
      memory_.invalidateLrs(addr, size);
    }

+------------------+
|      VeeR2       |
|  (Memory + LR)   |
+------------------+
|
v
+------------+
|   BUS      |   ← snoop point
+------------+
|         |
v         v
Memory      DMA

// Changes are being done bus.h to incoperate the above
a. Add an observer interface
b. Let VeeR2 wrapper implement it
c. Bus checks initiator identity
d. Bus notifies VeeR2 only when writer ≠ VeeR2
***************************************************************************/

struct PortMapping {
	uint64_t start;
	uint64_t end;
	sc_core::sc_module &module;

	// Optional live window source, for targets whose address range is defined by
	// a CSR rather than fixed at build time (the RTL crossbar re-reads those CSRs
	// per transaction, so a static range makes firmware reprogramming a no-op).
	// Returns the current inclusive [start, end]; when set it supersedes the
	// constructor's values, so a window can move anywhere, not just shrink.
	using window_fn = std::function<std::pair<uint64_t, uint64_t>()>;
	window_fn live_window;

	PortMapping(uint64_t start, uint64_t end, sc_core::sc_module &module) : start(start), end(end), module(module) {
		assert(end >= start);
	}

	PortMapping(window_fn window, sc_core::sc_module &module)
	    : start(0), end(0), module(module), live_window(std::move(window)) {
		const auto w = live_window();
		start = w.first;
		end = w.second;
	}

	// Samples a CSR-backed window once and caches it in start/end. The bus calls
	// contains() from decode() and then global_to_local()/to_string() on whichever
	// port matched, so resolving here is what makes those three agree on one
	// window: re-reading the CSRs in each of them would let a reprogram land
	// mid-transaction and translate an address against a window that never
	// matched. It also keeps it to one read per port per decode.
	//
	// Empty windows (a region size CSR of 0) match nothing, as in RTL. They are
	// expressed as hi < lo, which is why the check below is not just a range test.
	bool contains(uint64_t addr) {
		if (live_window) {
			const auto w = live_window();
			start = w.first;
			end   = w.second;
		}
		return end >= start && addr >= start && addr <= end;
	}

	// Both read the window cached by the preceding contains(). For a static
	// mapping that is simply the constructor's range.
	uint64_t global_to_local(uint64_t addr) {
		return addr - start;
	}

	std::string to_string() {
		std::stringstream ss;
		ss << module.name() << " " << std::hex << start << " " << std::hex << end;
		return ss.str();
	}
};

struct BusWriteObserver
{
    virtual void notifyWrite(uint64_t addr,
                             unsigned size,
                             int initiator_id) = 0;
};

template <unsigned int NR_OF_INITIATORS, unsigned int NR_OF_TARGETS>
struct SimpleBus : sc_core::sc_module {

#ifndef CSML_DEFAULT_VERBOSITY
#define CSML_DEFAULT_VERBOSITY 2
#endif

	CsmlLogger logger;
	csml_param<int> verbosity;
	// Tagged so the decode knows which initiator a transaction arrived on: a
	// real crossbar has a per-initiator connectivity matrix, and enforcing it
	// needs the initiator's identity, which the untagged socket discards.
	std::array<tlm_utils::simple_target_socket_tagged<SimpleBus>, NR_OF_INITIATORS> tsocks;

	std::array<tlm_utils::simple_initiator_socket<SimpleBus>, NR_OF_TARGETS> isocks;
	std::array<PortMapping *, NR_OF_TARGETS> ports;

	// Optional connectivity matrix.  Returns false when the initiator has no
	// route to the address, which the crossbar answers as a decode error just
	// like an unmapped address.  Left unset, every initiator reaches every
	// target (the historical behaviour).
	std::function<bool(unsigned initiator_id, uint64_t addr)> access_policy;

	bool break_on_transaction;

	SimpleBus(sc_core::sc_module_name name, bool trans_break)
	    : sc_module(name)
	    , verbosity("verbosity", CSML_DEFAULT_VERBOSITY)
        , break_on_transaction(trans_break) {
		// Initialize logger
		logger.setMaxVerbosity(verbosity.get_param_value());
		logger.setLogFormat("[%TIME%] [%LEVEL% %VERBOSITY%] [%MODULE%::%FUNCTION%] - %MESSAGE%");
		logger.setFunctionTrace(false);
   	        CSML_INFO(2, logger) << "SimpleBus module constructor - NR_OF_INITIATORS=" << NR_OF_INITIATORS  << " NR_OF_TARGETS=" << NR_OF_TARGETS << std::endl;
		for (unsigned i = 0; i < NR_OF_INITIATORS; ++i) {
			tsocks[i].register_b_transport(this, &SimpleBus::transport, i);
			tsocks[i].register_transport_dbg(this, &SimpleBus::transport_dbg, i);
		}
	}

	// Returns true when the initiator may reach the address.
	bool permitted(unsigned initiator_id, uint64_t addr) {
		return !access_policy || access_policy(initiator_id, addr);
	}

	int decode(uint64_t addr) {
		for (unsigned i = 0; i < NR_OF_TARGETS; ++i) {
			if (ports[i]->contains(addr))
				return i;
		}
		return -1;
	}

	void mapping_complete() {}

	static inline initiator_if *get_tlm_initiator(tlm::tlm_generic_payload &trans) {
		auto init_ext = trans.get_extension<initiator_ext>();
		if (init_ext != nullptr) {
			/* note: may be nullptr either */
			return init_ext->initiator;
		}
		return nullptr;
	}

	void transport(int initiator_id, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
		auto addr = trans.get_address();
		auto id = decode(addr);
		auto size = trans.get_data_length();

		if (id < 0) {
			trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
			return;
		}

		if (!permitted(unsigned(initiator_id), addr)) {
			CSML_INFO(3, logger) << "connectivity block: initiator " << initiator_id
			                     << " has no route to addr=0x" << std::hex << addr
			                     << " (" << ports[id]->to_string() << ")" << std::dec << std::endl;
			trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
			return;
		}

		// Check if its a write transcation
		// Invalidate the reservations (maintained by VeeR2), so that the core knows that there has been a write since
		// the last read by VeeR2
		bool isWrite = trans.is_write();

		if (isWrite) {
			for (auto* obs : observers_) {
				obs->notifyWrite(addr, size, id);
				CSML_DEBUG(5, logger) <<  "notifyWrite(to invalidate reservations) called from bus for addr=" << std::hex << addr << " initiator_id=" << id << std::dec << std::endl;
			}
		}

		if (break_on_transaction) {
			auto initiator = get_tlm_initiator(trans);
			if (initiator != nullptr) {
				initiator->halt();
			}
		}

		trans.set_address(ports[id]->global_to_local(addr));
		CSML_INFO(5, logger) << "addr=0x" << std::hex << addr << " portID:" << id << " Converted address=0x" <<  trans.get_address() << " isWrite=" << isWrite << std::dec << std::endl;
		isocks[id]->b_transport(trans, delay);
	}

	unsigned transport_dbg(int initiator_id, tlm::tlm_generic_payload &trans) {
		auto addr = trans.get_address();
		auto id = decode(addr);
		auto size = trans.get_data_length();
		if (id < 0) {
			trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
			return 0;
		}

		if (!permitted(unsigned(initiator_id), addr)) {
			trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
			return 0;
		}

		// Check if its a write transcation
		// Invalidate the reservations (maintained by VeeR2), so that the core knows that there has been a write since
		// the last read by VeeR2
		bool isWrite = trans.is_write();

		if (isWrite) {
			for (auto* obs : observers_) {
				obs->notifyWrite(addr, size, id);
				CSML_DEBUG(5, logger) <<  "notifyWrite(to invalidate reservations) called from bus for addr=" << std::hex << addr << " initiator_id=" << id << std::dec << std::endl;
			}
		}

		if (break_on_transaction) {
			auto initiator = get_tlm_initiator(trans);
			if (initiator != nullptr) {
				initiator->halt();
			}
		}
		trans.set_address(ports[id]->global_to_local(addr));
		CSML_INFO(5, logger) << "addr=0x" << std::hex << addr << " portID:" << id << " Converted address=0x" <<  trans.get_address() << " isWrite=" <<  trans.is_write() << std::dec << std::endl;
		isocks[id]->transport_dbg(trans);
		return trans.get_data_length();
	}

	void registerObserver(BusWriteObserver* obs) {
		observers_.push_back(obs);
	}

	private:
	std::vector<BusWriteObserver*> observers_;
};


#include "bus_lock_if.h"

/*
 * Use this adapter to attach peripherals with write access (e.g. DMA) to the bus.
 * This ensures that those peripherals do not violate the RISC-V LR/SC atomic semantic.
 */
struct PeripheralWriteConnector : sc_core::sc_module {
	tlm_utils::simple_target_socket<PeripheralWriteConnector> tsock;
	tlm_utils::simple_initiator_socket<PeripheralWriteConnector> isock;
	std::shared_ptr<bus_lock_if> bus_lock;

	PeripheralWriteConnector(sc_core::sc_module_name) {
		tsock.register_b_transport(this, &PeripheralWriteConnector::transport);
	}

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
		bus_lock->wait_until_unlocked();

		isock->b_transport(trans, delay);

		if (trans.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE)
			throw std::runtime_error("unable to find target port for address " + std::to_string(trans.get_address()));
	}
};

class BusLock : public bus_lock_if {
	bool locked = false;
	unsigned owner = 0;
	sc_core::sc_event lock_event;

   public:
	virtual void lock(unsigned hart_id) override {
		if (locked && (hart_id != owner)) {
			wait_until_unlocked();
		}

		assert(!locked || (hart_id == owner));
		locked = true;
		owner = hart_id;
	}

	virtual void unlock(unsigned hart_id) override {
		if (locked && (owner == hart_id)) {
			locked = false;
			lock_event.notify(sc_core::SC_ZERO_TIME);
		}
	}

	virtual bool is_locked() override {
		return locked;
	}

	virtual bool is_locked(unsigned hart_id) override {
		return locked && (owner == hart_id);
	}

	virtual void wait_until_unlocked() override {
		while (locked) sc_core::wait(lock_event);
	}
};

#endif  // RISCV_ISA_BUS_H
