/*
 * extension.h
 *
 *  Created on: Feb 3, 2026
 *      Author: ctr-sdangi
 */

#ifndef SYSTEMC_INCLUDE_EXTENSION_H_
#define SYSTEMC_INCLUDE_EXTENSION_H_


#include <systemc.h>
#include <tlm.h>
#include <vector>

class xspi_target_trans : public tlm::tlm_extension<xspi_target_trans> {
    public:
        ~xspi_target_trans() {
        }
    
        // Target Signals
        uint8_t xspi_target_opcode; // Input: XSPI target opcode
    
        // Clone method for deep copy
        virtual tlm_extension_base* clone() const override {
            // TODO: Implement proper logging for clone method not implemented
            return nullptr;
        }
    
        // Copy method
        virtual void copy_from(const tlm_extension_base& ext) override {
            // TODO: Implement proper logging for copy_from method not implemented
        }
    };
    



#endif /* SYSTEMC_INCLUDE_EXTENSION_H_ */
