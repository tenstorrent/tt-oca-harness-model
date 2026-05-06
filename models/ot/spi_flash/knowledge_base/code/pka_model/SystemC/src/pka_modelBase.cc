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
 * CHECKSUM:074080439127f0a721b4efee991511ffb74d0950
 ***************************************************************************/
 

 
 // Warning: This is an auto-generated file. All changes to this file will be overwritten.


#include <scml2.h>
#include <systemc>
#include <scmlinc/scml_property.h>
#include "scml2_objects.h"
#include "scml2_protocol_engines/reset/include/reset_engine.h"
#include "scml2_protocol_engines/interrupt/include/interrupt_engine.h"
#include "scml2_protocol_engines/tlm2_ft_target_port/include/tlm2_ft_target_port_pe.h"


#include "pka_modelBase.h"

namespace mylibrary {

#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif

pka_modelBase::CTRL_type::~CTRL_type() {};

pka_modelBase::CTRL_type::CTRL_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    PARTIAL_RADIX(sc_core::sc_gen_unique_name("PARTIAL_RADIX", true), *this, 0, 8),
    PARTIAL_RADIX_reset_value(0x0),
    BASE_RADIX(sc_core::sc_gen_unique_name("BASE_RADIX", true), *this, 8, 3),
    BASE_RADIX_reset_value(0x0),
    M521_MODE(sc_core::sc_gen_unique_name("M521_MODE", true), *this, 16, 5),
    M521_MODE_reset_value(0x0),
    STOP_RQST(sc_core::sc_gen_unique_name("STOP_RQST", true), *this, 27, 1),
    STOP_RQST_reset_value(0x0),
    GO(sc_core::sc_gen_unique_name("GO", true), *this, 31, 1),
    GO_reset_value(0x0) {}

pka_modelBase::ENTRY_PNT_type::~ENTRY_PNT_type() {};

pka_modelBase::ENTRY_PNT_type::ENTRY_PNT_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    ADDR(sc_core::sc_gen_unique_name("ADDR", true), *this, 0, 12),
    ADDR_reset_value(0x0) {}

pka_modelBase::RTN_CODE_type::~RTN_CODE_type() {};

pka_modelBase::RTN_CODE_type::RTN_CODE_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    STOP_REASON(sc_core::sc_gen_unique_name("STOP_REASON", true), *this, 16, 8),
    STOP_REASON_reset_value(0x0),
    DPA_EN(sc_core::sc_gen_unique_name("DPA_EN", true), *this, 27, 1),
    DPA_EN_reset_value(0x0),
    ZERO(sc_core::sc_gen_unique_name("ZERO", true), *this, 28, 1),
    ZERO_reset_value(0x0),
    IRQ(sc_core::sc_gen_unique_name("IRQ", true), *this, 30, 1),
    IRQ_reset_value(0x0),
    BUSY(sc_core::sc_gen_unique_name("BUSY", true), *this, 31, 1),
    BUSY_reset_value(0x0) {}

pka_modelBase::BUILD_CONFIG_type::~BUILD_CONFIG_type() {};

pka_modelBase::BUILD_CONFIG_type::BUILD_CONFIG_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    BANK_SW_A(sc_core::sc_gen_unique_name("BANK_SW_A", true), *this, 0, 2),
    BANK_SW_A_reset_value(0x0),
    BANK_SW_B(sc_core::sc_gen_unique_name("BANK_SW_B", true), *this, 2, 2),
    BANK_SW_B_reset_value(0x0),
    BANK_SW_C(sc_core::sc_gen_unique_name("BANK_SW_C", true), *this, 4, 2),
    BANK_SW_C_reset_value(0x0),
    BANK_SW_D(sc_core::sc_gen_unique_name("BANK_SW_D", true), *this, 6, 2),
    BANK_SW_D_reset_value(0x0),
    FW_RAM_SIZE(sc_core::sc_gen_unique_name("FW_RAM_SIZE", true), *this, 8, 3),
    FW_RAM_SIZE_reset_value(0x0),
    FW_ROM_SIZE(sc_core::sc_gen_unique_name("FW_ROM_SIZE", true), *this, 11, 3),
    FW_ROM_SIZE_reset_value(0x4),
    ECC_MAX(sc_core::sc_gen_unique_name("ECC_MAX", true), *this, 14, 2),
    ECC_MAX_reset_value(0x3),
    RSA_MAX(sc_core::sc_gen_unique_name("RSA_MAX", true), *this, 16, 3),
    RSA_MAX_reset_value(0x4),
    ALU_WIDTH(sc_core::sc_gen_unique_name("ALU_WIDTH", true), *this, 19, 2),
    ALU_WIDTH_reset_value(0x2),
    TA_DPA_SUPPORT(sc_core::sc_gen_unique_name("TA_DPA_SUPPORT", true), *this, 21, 2),
    TA_DPA_SUPPORT_reset_value(0x2),
    FORMAT_TYPE(sc_core::sc_gen_unique_name("FORMAT_TYPE", true), *this, 30, 2),
    FORMAT_TYPE_reset_value(0x2) {}

pka_modelBase::STACK_PNTR_type::~STACK_PNTR_type() {};

pka_modelBase::STACK_PNTR_type::STACK_PNTR_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    PNTR(sc_core::sc_gen_unique_name("PNTR", true), *this, 0, 12),
    PNTR_reset_value(0x0) {}

pka_modelBase::INSTR_SINCE_GO_type::~INSTR_SINCE_GO_type() {};

pka_modelBase::INSTR_SINCE_GO_type::INSTR_SINCE_GO_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    INSTRS(sc_core::sc_gen_unique_name("INSTRS", true), *this, 0, 32),
    INSTRS_reset_value(0x0) {}

pka_modelBase::CONFIG_type::~CONFIG_type() {};

pka_modelBase::CONFIG_type::CONFIG_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    ALT_ACCESS(sc_core::sc_gen_unique_name("ALT_ACCESS", true), *this, 0, 1),
    ALT_ACCESS_reset_value(0x0),
    ENDIAN_SWAP(sc_core::sc_gen_unique_name("ENDIAN_SWAP", true), *this, 26, 1),
    ENDIAN_SWAP_reset_value(0x0) {}

pka_modelBase::STAT_type::~STAT_type() {};

pka_modelBase::STAT_type::STAT_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    DONE(sc_core::sc_gen_unique_name("DONE", true), *this, 30, 1),
    DONE_reset_value(0x0) {}

pka_modelBase::FLAGS_type::~FLAGS_type() {};

pka_modelBase::FLAGS_type::FLAGS_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    Z(sc_core::sc_gen_unique_name("Z", true), *this, 0, 1),
    Z_reset_value(0x0),
    M(sc_core::sc_gen_unique_name("M", true), *this, 1, 1),
    M_reset_value(0x0),
    B(sc_core::sc_gen_unique_name("B", true), *this, 2, 1),
    B_reset_value(0x0),
    C(sc_core::sc_gen_unique_name("C", true), *this, 3, 1),
    C_reset_value(0x0),
    F0(sc_core::sc_gen_unique_name("F0", true), *this, 4, 1),
    F0_reset_value(0x0),
    F1(sc_core::sc_gen_unique_name("F1", true), *this, 5, 1),
    F1_reset_value(0x0),
    F2(sc_core::sc_gen_unique_name("F2", true), *this, 6, 1),
    F2_reset_value(0x0),
    F3(sc_core::sc_gen_unique_name("F3", true), *this, 7, 1),
    F3_reset_value(0x0) {}

pka_modelBase::WATCHDOG_type::~WATCHDOG_type() {};

pka_modelBase::WATCHDOG_type::WATCHDOG_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    RELOAD(sc_core::sc_gen_unique_name("RELOAD", true), *this, 0, 32),
    RELOAD_reset_value(0xffffffff) {}

pka_modelBase::CYCLES_SINCE_GO_type::~CYCLES_SINCE_GO_type() {};

pka_modelBase::CYCLES_SINCE_GO_type::CYCLES_SINCE_GO_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    CYCLES(sc_core::sc_gen_unique_name("CYCLES", true), *this, 0, 32),
    CYCLES_reset_value(0x0) {}

pka_modelBase::INDEX_I_type::~INDEX_I_type() {};

pka_modelBase::INDEX_I_type::INDEX_I_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    INDEX(sc_core::sc_gen_unique_name("INDEX", true), *this, 0, 16),
    INDEX_reset_value(0x0) {}

pka_modelBase::INDEX_J_type::~INDEX_J_type() {};

pka_modelBase::INDEX_J_type::INDEX_J_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    INDEX(sc_core::sc_gen_unique_name("INDEX", true), *this, 0, 16),
    INDEX_reset_value(0x0) {}

pka_modelBase::INDEX_K_type::~INDEX_K_type() {};

pka_modelBase::INDEX_K_type::INDEX_K_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    INDEX(sc_core::sc_gen_unique_name("INDEX", true), *this, 0, 16),
    INDEX_reset_value(0x0) {}

pka_modelBase::INDEX_L_type::~INDEX_L_type() {};

pka_modelBase::INDEX_L_type::INDEX_L_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    INDEX(sc_core::sc_gen_unique_name("INDEX", true), *this, 0, 16),
    INDEX_reset_value(0x0) {}

pka_modelBase::IRQ_EN_type::~IRQ_EN_type() {};

pka_modelBase::IRQ_EN_type::IRQ_EN_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    IE(sc_core::sc_gen_unique_name("IE", true), *this, 30, 1),
    IE_reset_value(0x0) {}

pka_modelBase::JMP_PROB_type::~JMP_PROB_type() {};

pka_modelBase::JMP_PROB_type::JMP_PROB_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    PROB(sc_core::sc_gen_unique_name("PROB", true), *this, 0, 13),
    PROB_reset_value(0x0) {}

pka_modelBase::JMP_PROB_LFSR_type::~JMP_PROB_LFSR_type() {};

pka_modelBase::JMP_PROB_LFSR_type::JMP_PROB_LFSR_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    INIT(sc_core::sc_gen_unique_name("INIT", true), *this, 0, 13),
    INIT_reset_value(0x1) {}

pka_modelBase::BANK_SW_A_type::~BANK_SW_A_type() {};

pka_modelBase::BANK_SW_A_type::BANK_SW_A_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    BANK_SW(sc_core::sc_gen_unique_name("BANK_SW", true), *this, 0, 2) {}

pka_modelBase::BANK_SW_B_type::~BANK_SW_B_type() {};

pka_modelBase::BANK_SW_B_type::BANK_SW_B_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    BANK_SW(sc_core::sc_gen_unique_name("BANK_SW", true), *this, 0, 2) {}

pka_modelBase::BANK_SW_C_type::~BANK_SW_C_type() {};

pka_modelBase::BANK_SW_C_type::BANK_SW_C_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    BANK_SW(sc_core::sc_gen_unique_name("BANK_SW", true), *this, 0, 2) {}

pka_modelBase::BANK_SW_D_type::~BANK_SW_D_type() {};

pka_modelBase::BANK_SW_D_type::BANK_SW_D_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset) :
    scml2::reg< unsigned int >(name, memory, offset),
    BANK_SW(sc_core::sc_gen_unique_name("BANK_SW", true), *this, 0, 2) {}


#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif


}  // end of namespace mylibrary
