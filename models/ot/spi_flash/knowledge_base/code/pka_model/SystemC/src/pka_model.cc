/***************************************************************************
 * Copyright 1996-2025 Synopsys, Inc.
 *
 * This Synopsys software and all associated documentation are proprietary
 * to Synopsys, Inc. and may only be used pursuant to the terms and
 * conditions of a written license agreement with Synopsys, Inc.
 * All other use, reproduction, modification, or distribution of the
 * Synopsys software or the associated documentation is strictly prohibited.
 ***************************************************************************/
 

/***************************************************************************
 * Generated snippet, used for detecting user edits.
 * CHECKSUM:56330f5403ebdaf4ed087bc7cabe61b2386d1a04
 ***************************************************************************/
 
#include "pka_model.h"

namespace mylibrary {

pka_model::pka_model(sc_core::sc_module_name name) : pka_modelBase(name)  {
  
}

void pka_model::handle_write_bank_A(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) {
  // FIXME: Default implementation. Implement this.
  this->bank_A.transport_without_triggering_callbacks(payload);
}

void pka_model::handle_write_bank_B(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) {
  // FIXME: Default implementation. Implement this.
  this->bank_B.transport_without_triggering_callbacks(payload);
}

void pka_model::handle_write_bank_C(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) {
  // FIXME: Default implementation. Implement this.
  this->bank_C.transport_without_triggering_callbacks(payload);
}

void pka_model::handle_write_bank_D(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) {
  // FIXME: Default implementation. Implement this.
  this->bank_D.transport_without_triggering_callbacks(payload);
}

#ifdef ACCELLERA_SYSTEMC
void pka_model::process_params(const std::map<std::string, std::string> & params){
for (const auto &it : params)
{
    const std::string &key   = it.first;
    const std::string &value = it.second;

    unsigned long v = std::stoul(value);

    if (key == "F_W_MemoryMix") {
        this->F_W_MemoryMix = v;
    }
    else if (key == "F_W_RamMemorySize") {
        this->F_W_RamMemorySize = v;
    }
    else if (key == "F_W_RomMemorySize") {
        this->F_W_RomMemorySize = v;
    }
    else if (key == "EnableBankswitchA") {
        this->EnableBankswitchA = v;
    }
    else if (key == "Include_RSA_Functions") {
        this->Include_RSA_Functions = v;
    }
}
}
#endif

void pka_model::handle_write_pka_fw(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) {
  // FIXME: Default implementation. Implement this.
  std::cout << "handle_write_pka_fw: " << std::endl;
  this->pka_fw.transport_without_triggering_callbacks(payload);
}

bool pka_model::handle_write_FLAGS(const unsigned int &value, const unsigned int &byteEnables, sc_core::sc_time &time) {
  (void)time;
  cout << "handle_write_FLAGS: " << value << " byteEnables: " << byteEnables << " time: " << time << std::endl;
  
  unsigned int oldVal = static_cast<unsigned int>(this->FLAGS);
  this->FLAGS.put((oldVal & ~byteEnables) | (value & byteEnables));
  return true;
}


}  // end of namespace mylibrary
