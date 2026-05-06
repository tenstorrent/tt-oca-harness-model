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
 * CHECKSUM:193cbbd3130c731af195bb702d106410fca77f25
 ***************************************************************************/
 

 
 // Warning: This is an auto-generated file. All changes to this file will be overwritten.


#pragma once

#include <scml2.h>
#include <systemc>
#include <scmlinc/scml_property.h>
#include "scml2_objects.h"
#include "scml2_protocol_engines/reset/include/reset_engine.h"
#include "scml2_protocol_engines/interrupt/include/interrupt_engine.h"
#include "scml2_protocol_engines/tlm2_ft_target_port/include/tlm2_ft_target_port_pe.h"


namespace mylibrary {


class pka_modelCovermodelBase;


class pka_modelCovermodel;


class pka_model;

/**
 * This empty model example contains no predefined interfaces or registers.
It allows full flexibility in defining arbitrary components as it generates a minimal SystemC skeleton.
 * 
 * \version 1.0
 *
 * \par Company
 *   example.org
 */
#ifdef _WIN32
#pragma optimize("g",off)
#else
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif


class pka_modelBase : public sc_core::sc_module {
    typedef pka_modelBase ModelBaseType;
  public:
#if (defined SYSTEMC_VERSION && SYSTEMC_VERSION <= 20221128 /*2.3.4*/)
    SC_HAS_PROCESS(pka_modelBase);
#endif

    pka_modelBase(sc_core::sc_module_name name) 
        : sc_core::sc_module(name)
        , F_W_MemoryMix("F_W_MemoryMix", 0)
        , F_W_RamMemorySize("F_W_RamMemorySize", 11)
        , F_W_RomMemorySize("F_W_RomMemorySize", 11)
        , EnableBankswitchA("EnableBankswitchA", 1)
        , Include_RSA_Functions("Include_RSA_Functions", 0)
        , p_reset("p_reset")
        , p_intr("p_intr")
        , p_mem_socket("p_mem_socket")
        , p_mem_socket_router("p_mem_socket_router")
        , p_reset_protocol_engine("p_reset_protocol_engine", p_reset)
        , p_intr_protocol_engine("p_intr_protocol_engine", p_intr)
        , p_mem_socket_protocol_engine("p_mem_socket_protocol_engine", p_mem_socket)
        , sep_pka("sep_pka", (0x8000) / scml2::sizeOf<unsigned int >())
        , bank_A("bank_A", sep_pka, (0x400) / scml2::sizeOf<unsigned int >(), (0x400) / scml2::sizeOf<unsigned int >())
        , bank_B("bank_B", sep_pka, (0x800) / scml2::sizeOf<unsigned int >(), (1024) / scml2::sizeOf<unsigned int >())
        , bank_C("bank_C", sep_pka, (0xC00) / scml2::sizeOf<unsigned int >(), (1024) / scml2::sizeOf<unsigned int >())
        , bank_D("bank_D", sep_pka, (0x1000) / scml2::sizeOf<unsigned int >(), (2048) / scml2::sizeOf<unsigned int >())
        , pka_fw("pka_fw", sep_pka, (0x4000) / scml2::sizeOf<unsigned int >(), (16384) / scml2::sizeOf<unsigned int >())
        , CTRL(sc_core::sc_gen_unique_name("CTRL", true), sep_pka, (0x0) / scml2::sizeOf<unsigned int >())
        , ENTRY_PNT(sc_core::sc_gen_unique_name("ENTRY_PNT", true), sep_pka, (0x4) / scml2::sizeOf<unsigned int >())
        , RTN_CODE(sc_core::sc_gen_unique_name("RTN_CODE", true), sep_pka, (0x8) / scml2::sizeOf<unsigned int >())
        , BUILD_CONFIG(sc_core::sc_gen_unique_name("BUILD_CONFIG", true), sep_pka, (0xc) / scml2::sizeOf<unsigned int >())
        , STACK_PNTR(sc_core::sc_gen_unique_name("STACK_PNTR", true), sep_pka, (0x10) / scml2::sizeOf<unsigned int >())
        , INSTR_SINCE_GO(sc_core::sc_gen_unique_name("INSTR_SINCE_GO", true), sep_pka, (0x14) / scml2::sizeOf<unsigned int >())
        , CONFIG(sc_core::sc_gen_unique_name("CONFIG", true), sep_pka, (0x1c) / scml2::sizeOf<unsigned int >())
        , STAT(sc_core::sc_gen_unique_name("STAT", true), sep_pka, (0x20) / scml2::sizeOf<unsigned int >())
        , FLAGS(sc_core::sc_gen_unique_name("FLAGS", true), sep_pka, (0x24) / scml2::sizeOf<unsigned int >())
        , WATCHDOG(sc_core::sc_gen_unique_name("WATCHDOG", true), sep_pka, (0x28) / scml2::sizeOf<unsigned int >())
        , CYCLES_SINCE_GO(sc_core::sc_gen_unique_name("CYCLES_SINCE_GO", true), sep_pka, (0x2c) / scml2::sizeOf<unsigned int >())
        , INDEX_I(sc_core::sc_gen_unique_name("INDEX_I", true), sep_pka, (0x30) / scml2::sizeOf<unsigned int >())
        , INDEX_J(sc_core::sc_gen_unique_name("INDEX_J", true), sep_pka, (0x34) / scml2::sizeOf<unsigned int >())
        , INDEX_K(sc_core::sc_gen_unique_name("INDEX_K", true), sep_pka, (0x38) / scml2::sizeOf<unsigned int >())
        , INDEX_L(sc_core::sc_gen_unique_name("INDEX_L", true), sep_pka, (0x3c) / scml2::sizeOf<unsigned int >())
        , IRQ_EN(sc_core::sc_gen_unique_name("IRQ_EN", true), sep_pka, (0x40) / scml2::sizeOf<unsigned int >())
        , JMP_PROB(sc_core::sc_gen_unique_name("JMP_PROB", true), sep_pka, (0x44) / scml2::sizeOf<unsigned int >())
        , JMP_PROB_LFSR(sc_core::sc_gen_unique_name("JMP_PROB_LFSR", true), sep_pka, (0x48) / scml2::sizeOf<unsigned int >())
        , BANK_SW_A(sc_core::sc_gen_unique_name("BANK_SW_A", true), sep_pka, (0x50) / scml2::sizeOf<unsigned int >())
        , BANK_SW_A_reset_value(0)
        , BANK_SW_A_reset_mask(4294967295)
        , BANK_SW_B(sc_core::sc_gen_unique_name("BANK_SW_B", true), sep_pka, (0x54) / scml2::sizeOf<unsigned int >())
        , BANK_SW_B_reset_value(0)
        , BANK_SW_B_reset_mask(4294967295)
        , BANK_SW_C(sc_core::sc_gen_unique_name("BANK_SW_C", true), sep_pka, (0x58) / scml2::sizeOf<unsigned int >())
        , BANK_SW_C_reset_value(0)
        , BANK_SW_C_reset_mask(4294967295)
        , BANK_SW_D(sc_core::sc_gen_unique_name("BANK_SW_D", true), sep_pka, (0x5c) / scml2::sizeOf<unsigned int >())
        , BANK_SW_D_reset_value(0)
        , BANK_SW_D_reset_mask(4294967295)
    {
      p_mem_socket_router.mappings.resize(1);
      p_mem_socket_protocol_engine.intercepts.resize(0);
      p_mem_socket_router.bound_on = p_mem_socket_protocol_engine;
      p_mem_socket_router.mappings[0].base = 0x0;
      p_mem_socket_router.mappings[0].size = sep_pka.get_size()*sep_pka.get_width();
      p_mem_socket_router.mappings[0].destination = sep_pka;
      /* p_mem_socket_router.mappings[0].offset = ; */
      p_mem_socket_router.mappings[0].type = scml2::objects::RW;
      p_mem_socket_router.mappings[0].active = true;
      /* p_mem_socket_router.mappings[0].name = ; */
      p_mem_socket_router.finalize_construction();
      p_reset_protocol_engine.active_level = false;
      p_reset_protocol_engine.finalize_construction();
      p_intr_protocol_engine.active_level = false;
      /* p_intr_protocol_engine.enable = ; */
      p_intr_protocol_engine.finalize_construction();
      p_mem_socket_protocol_engine.abstraction = scml2::LT;
      #ifdef SYNOPSYS_SYSTEMC
      p_mem_socket_protocol_engine.consume_annotated_time = false;
      #endif
      p_mem_socket_protocol_engine.finalize_construction();
      scml2::set_write_ignore_restriction(RTN_CODE.STOP_REASON);
      scml2::set_write_ignore_restriction(RTN_CODE.DPA_EN);
      scml2::set_write_ignore_restriction(RTN_CODE.ZERO);
      scml2::set_write_ignore_restriction(RTN_CODE.IRQ);
      scml2::set_write_ignore_restriction(RTN_CODE.BUSY);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.BANK_SW_A);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.BANK_SW_B);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.BANK_SW_C);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.BANK_SW_D);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.FW_RAM_SIZE);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.FW_ROM_SIZE);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.ECC_MAX);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.RSA_MAX);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.ALU_WIDTH);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.TA_DPA_SUPPORT);
      scml2::set_write_ignore_restriction(BUILD_CONFIG.FORMAT_TYPE);
      scml2::set_write_ignore_restriction(INSTR_SINCE_GO.INSTRS);
      scml2::set_clear_on_write_1(STAT.DONE);
      scml2::set_write_ignore_restriction(CYCLES_SINCE_GO.CYCLES);
      scml2::set_ignore_restriction<unsigned int, scml2::reg>(BANK_SW_A, (unsigned int)0ull, (unsigned int)4294967292ull);
      scml2::set_ignore_restriction<unsigned int, scml2::reg>(BANK_SW_B, (unsigned int)0ull, (unsigned int)4294967292ull);
      scml2::set_ignore_restriction<unsigned int, scml2::reg>(BANK_SW_C, (unsigned int)0ull, (unsigned int)4294967292ull);
      scml2::set_ignore_restriction<unsigned int, scml2::reg>(BANK_SW_D, (unsigned int)0ull, (unsigned int)4294967292ull);
      scml2::set_write_callback<unsigned int, scml2::memory_alias>(bank_A, SCML2_CALLBACK(handle_write_bank_A), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::memory_alias>(bank_B, SCML2_CALLBACK(handle_write_bank_B), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::memory_alias>(bank_C, SCML2_CALLBACK(handle_write_bank_C), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::memory_alias>(bank_D, SCML2_CALLBACK(handle_write_bank_D), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::memory_alias>(pka_fw, SCML2_CALLBACK(handle_write_pka_fw), scml2::AUTO_SYNCING);
      scml2::set_write_callback<unsigned int, scml2::reg>(FLAGS, SCML2_CALLBACK(handle_write_FLAGS), scml2::AUTO_SYNCING);

    }
    
// LCOV_EXCL_START
    virtual ~pka_modelBase() {
      
    }
// LCOV_EXCL_STOP    

  protected:
    /**
     * This method resets all interfaces, registers, and bitfields to their reset value.
     * It is typically called from end_of_elaboration(), and connected to the reset port.
     */
    
    virtual void reset_model() {
      reset_model_if_available(p_mem_socket_router);
      CTRL.PARTIAL_RADIX = CTRL.PARTIAL_RADIX_reset_value;
      CTRL.BASE_RADIX = CTRL.BASE_RADIX_reset_value;
      CTRL.M521_MODE = CTRL.M521_MODE_reset_value;
      CTRL.STOP_RQST = CTRL.STOP_RQST_reset_value;
      CTRL.GO = CTRL.GO_reset_value;
      ENTRY_PNT.ADDR = ENTRY_PNT.ADDR_reset_value;
      RTN_CODE.STOP_REASON = RTN_CODE.STOP_REASON_reset_value;
      RTN_CODE.DPA_EN = RTN_CODE.DPA_EN_reset_value;
      RTN_CODE.ZERO = RTN_CODE.ZERO_reset_value;
      RTN_CODE.IRQ = RTN_CODE.IRQ_reset_value;
      RTN_CODE.BUSY = RTN_CODE.BUSY_reset_value;
      BUILD_CONFIG.BANK_SW_A = BUILD_CONFIG.BANK_SW_A_reset_value;
      BUILD_CONFIG.BANK_SW_B = BUILD_CONFIG.BANK_SW_B_reset_value;
      BUILD_CONFIG.BANK_SW_C = BUILD_CONFIG.BANK_SW_C_reset_value;
      BUILD_CONFIG.BANK_SW_D = BUILD_CONFIG.BANK_SW_D_reset_value;
      BUILD_CONFIG.FW_RAM_SIZE = BUILD_CONFIG.FW_RAM_SIZE_reset_value;
      BUILD_CONFIG.FW_ROM_SIZE = BUILD_CONFIG.FW_ROM_SIZE_reset_value;
      BUILD_CONFIG.ECC_MAX = BUILD_CONFIG.ECC_MAX_reset_value;
      BUILD_CONFIG.RSA_MAX = BUILD_CONFIG.RSA_MAX_reset_value;
      BUILD_CONFIG.ALU_WIDTH = BUILD_CONFIG.ALU_WIDTH_reset_value;
      BUILD_CONFIG.TA_DPA_SUPPORT = BUILD_CONFIG.TA_DPA_SUPPORT_reset_value;
      BUILD_CONFIG.FORMAT_TYPE = BUILD_CONFIG.FORMAT_TYPE_reset_value;
      STACK_PNTR.PNTR = STACK_PNTR.PNTR_reset_value;
      INSTR_SINCE_GO.INSTRS = INSTR_SINCE_GO.INSTRS_reset_value;
      CONFIG.ALT_ACCESS = CONFIG.ALT_ACCESS_reset_value;
      CONFIG.ENDIAN_SWAP = CONFIG.ENDIAN_SWAP_reset_value;
      STAT.DONE = STAT.DONE_reset_value;
      FLAGS.Z = FLAGS.Z_reset_value;
      FLAGS.M = FLAGS.M_reset_value;
      FLAGS.B = FLAGS.B_reset_value;
      FLAGS.C = FLAGS.C_reset_value;
      FLAGS.F0 = FLAGS.F0_reset_value;
      FLAGS.F1 = FLAGS.F1_reset_value;
      FLAGS.F2 = FLAGS.F2_reset_value;
      FLAGS.F3 = FLAGS.F3_reset_value;
      WATCHDOG.RELOAD = WATCHDOG.RELOAD_reset_value;
      CYCLES_SINCE_GO.CYCLES = CYCLES_SINCE_GO.CYCLES_reset_value;
      INDEX_I.INDEX = INDEX_I.INDEX_reset_value;
      INDEX_J.INDEX = INDEX_J.INDEX_reset_value;
      INDEX_K.INDEX = INDEX_K.INDEX_reset_value;
      INDEX_L.INDEX = INDEX_L.INDEX_reset_value;
      IRQ_EN.IE = IRQ_EN.IE_reset_value;
      JMP_PROB.PROB = JMP_PROB.PROB_reset_value;
      JMP_PROB_LFSR.INIT = JMP_PROB_LFSR.INIT_reset_value;
      BANK_SW_A = (BANK_SW_A & ~BANK_SW_A_reset_mask) | (BANK_SW_A_reset_value & BANK_SW_A_reset_mask);
      BANK_SW_B = (BANK_SW_B & ~BANK_SW_B_reset_mask) | (BANK_SW_B_reset_value & BANK_SW_B_reset_mask);
      BANK_SW_C = (BANK_SW_C & ~BANK_SW_C_reset_mask) | (BANK_SW_C_reset_value & BANK_SW_C_reset_mask);
      BANK_SW_D = (BANK_SW_D & ~BANK_SW_D_reset_mask) | (BANK_SW_D_reset_value & BANK_SW_D_reset_mask);
    }
    
    virtual void end_of_elaboration() {
      sc_core::sc_module::end_of_elaboration();
      reset_model();
    }
    
    ModelBaseType& model() {
      return *this;
    }
  
    virtual void handle_write_bank_A(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) = 0;
    virtual void handle_write_bank_B(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) = 0;
    virtual void handle_write_bank_C(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) = 0;
    virtual void handle_write_bank_D(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) = 0;
    virtual void handle_write_pka_fw(tlm::tlm_generic_payload& payload, sc_core::sc_time& time) = 0;
    virtual bool handle_write_FLAGS(const unsigned int &value, const unsigned int &byteEnables, sc_core::sc_time &time) = 0;


  protected:
    /* Sets the type of memory used for F/W storage to be pure RAM, pure ROM, or a mixture of RAM and ROM. */
    scml_property<unsigned int> F_W_MemoryMix;
    /* Sets the size of the F/W RAM memory. NOTE: Total F/W memory limited to 4096 32-bit words. If F/W Memory Mix is set for mixed F/W RAM and ROM, must limit both F/W RAM Memory Size and F/W ROM Memory Size each to 2048 words or less. */
    scml_property<unsigned int> F_W_RamMemorySize;
    /* Sets the size of the F/W ROM memory. NOTE: Total F/W memory limited to 4096 32-bit words. If F/W Memory Mix is set for mixed F/W RAM and ROM, must limit both F/W RAM Memory Size and F/W ROM Memory Size each to 2048 words or less. */
    scml_property<unsigned int> F_W_RomMemorySize;
    /* Enable 1 additional shadow bank on data memory A. NOTE: Data memory A bankswitching must be enabled in order to add the ECC- Shamir and ECC-521 Shamir F/W components. */
    scml_property<bool> EnableBankswitchA;
    /* Includes or eliminates RSA F/W functions. */
    scml_property<bool> Include_RSA_Functions;

  public:
    sc_core::sc_in<bool> p_reset;
    sc_core::sc_out<bool> p_intr;
    scml2::ft_target_socket<32> p_mem_socket;

  protected:
    friend class pka_modelCovermodelBase;
    friend class pka_modelCovermodel;
    friend class pka_model;

    struct CTRL_type : public scml2::reg< unsigned int > {
      ~CTRL_type();
      CTRL_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Selects a partial field from within the base radix.
       * The bits operate as a kind of mask, allowing a sub-portion of the full radix to be performed.
       * They operate by enabling only that number of words of the operand to be processed.
       * An example for an instance with a 32-bit memory word; if the CTRL_BASE_RADIX bits are set to 2 to indicate a 256-bit operand size, and the PARTIAL_RADIX bits are set to 5, then 5 words of the 8 word operand will be processed. In effect, limiting the operand radix to 5/8 * 256 or 160 bits.
       * The PARTIAL_RADIX bits are the only bits in this register that are sensitive to the configured ALU width.
       *  - If the ALU width is 32 bits, set this field to 17 to enable Mersenne M-521 mode (ceiling(521/32) = 17).
       *  - If the ALU width is 64 bits, set this field to 18 to enable Mersenne M-521 mode (ceiling(521/64)*(64/32) = 18).
       *  - If the ALU width is 128 bits, set this field to 20 to enable Mersenne M-521 mode (ceiling(521/128)*(128/32) = 20).
       */
      scml2::bitfield< unsigned int > PARTIAL_RADIX;
      unsigned int PARTIAL_RADIX_reset_value;
      /**
       * Sets the size of the base operand size (radix).
       * The maximum size of this field is dictated by the actual parameterized hardware configuration.
       *    2 = 256-bit
       *    3 = 512-bit
       *    4 = 1024-bit
       *    5 = 2048-bit
       *    6 = 4096-bit
       *    others = reserved
       */
      scml2::bitfield< unsigned int > BASE_RADIX;
      unsigned int BASE_RADIX_reset_value;
      /**
       * Sets operating to Mersenne M-521.
       * Set field to 9 for M-521 mode. Set to 0 for all other operation sizes!
       * This field is only present in specific configurations.
       */
      scml2::bitfield< unsigned int > M521_MODE;
      unsigned int M521_MODE_reset_value;
      /* Requests PKA to halt at the next instruction boundary. */
      scml2::bitfield< unsigned int > STOP_RQST;
      unsigned int STOP_RQST_reset_value;
      /**
       * Starts the PKA operating on the current data and command set.
       * The PKA will begin executing instructions from the firmware address indicated in the ENTRY_PNT.ADDR field.
       */
      scml2::bitfield< unsigned int > GO;
      unsigned int GO_reset_value;
    };
    struct ENTRY_PNT_type : public scml2::reg< unsigned int > {
      ~ENTRY_PNT_type();
      ENTRY_PNT_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Indicates the start instruction of the function to be executed.
       * This register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA.
       * This register will be updated as the PKA is executing and will point at the next instruction to fetch when the PKA has stopped. This can be used in conjunction with either the CTRL.STOP_RQST bit or the watchdog register to run the PKA in a debug mode.
       * The address is a word index relative to the start of firmware memory.
       */
      scml2::bitfield< unsigned int > ADDR;
      unsigned int ADDR_reset_value;
    };
    struct RTN_CODE_type : public scml2::reg< unsigned int > {
      ~RTN_CODE_type();
      RTN_CODE_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Indicates why the PKA stopped.
       * If the PKA stopped due to a normal HALT instruction, this will indicate as 0. Any other reason will receive a non-0 value.
       * This field will be static and stable after the PKA has finished executing a program and will remain so until the PKA is restarted again.
       * If the PKA has finished an operation with the STOP_REASON other than 0, an abnormal stop condition has occurred. Under an abnormal stop condition, it is important to clear the STACK_PNTR register prior to starting any new operation. Additionally if any of the optional bankswitch registers are present, they must be set to 0 after an abnormal stop condition as well.
       * NOTE: This latter requirement of clearing all existing bankswitch registers MUST be done prior to loading any new data!
       *    0 = Normal stop
       *    1 = Invalid op-code
       *    2 = Stack underflow
       *    3 = Stack overflow
       *    4 = Watchdog
       *    5 = Host request (CTRL.STOP_RQST)
       *    6 = Reserved
       *    7 = Reserved
       *    8 = Memory port collision
       *    9 = Reserved
       *    others = reserved for firmware
       */
      scml2::bitfield< unsigned int > STOP_REASON;
      unsigned int STOP_REASON_reset_value;
      /**
       * Indicates that the core is capable of operating in TA and/or DPA mode.
       * If ELP_CLUE_SIDE_CHANNEL_MODE configuration setting is enabled at build-time, then this bit reflects the logical inverse of the I_dpa_disable I/O port. In other words, if ELP_CLUE_DPA_MODE is enabled and the I_dpa_disable port is driven to 0, this bit will read back as 1.
       * Conversely, if either ELP_CLUE_SIDE_CHANNEL_MODE is disabled, or ELP_CLUE_SIDE_CHANNEL_MODE is enabled and the I_dpa_disable I/O port is driven to 1, this bit will read back as 0.
       */
      scml2::bitfield< unsigned int > DPA_EN;
      unsigned int DPA_EN_reset_value;
      /**
       * Indicates the state of the zero flag in the sequencer.
       * Note: This bit is readable and also writable through the FLAGS.Z bit.
       */
      scml2::bitfield< unsigned int > ZERO;
      unsigned int ZERO_reset_value;
      /**
       * Indicates when the PKA has completed its task.
       * It is a R/O reflection of the STAT.IRQ bit.
       */
      scml2::bitfield< unsigned int > IRQ;
      unsigned int IRQ_reset_value;
      /* Indicates that the PKA is actively executing a program. */
      scml2::bitfield< unsigned int > BUSY;
      unsigned int BUSY_reset_value;
    };
    struct BUILD_CONFIG_type : public scml2::reg< unsigned int > {
      ~BUILD_CONFIG_type();
      BUILD_CONFIG_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Indicates how many bankswitch regions are present within A region.
       * 0 = 1 bank (only primary, no alternate)
       * 1 = 2 banks (primary and 1 alternate)
       * 2 = 4 banks (primary and 3 alternates)
       * others = reserved
       */
      scml2::bitfield< unsigned int > BANK_SW_A;
      unsigned int BANK_SW_A_reset_value;
      /**
       * Indicates how many bankswitch regions are present within B region.
       * 0 = 1 bank (only primary, no alternate)
       * 1 = 2 banks (primary and 1 alternate)
       * 2 = 4 banks (primary and 3 alternates)
       * others = reserved
       */
      scml2::bitfield< unsigned int > BANK_SW_B;
      unsigned int BANK_SW_B_reset_value;
      /**
       * Indicates how many bankswitch regions are present within C region.
       * 0 = 1 bank (only primary, no alternate)
       * 1 = 2 banks (primary and 1 alternate)
       * 2 = 4 banks (primary and 3 alternates)
       * others = reserved
       */
      scml2::bitfield< unsigned int > BANK_SW_C;
      unsigned int BANK_SW_C_reset_value;
      /**
       * Indicates how many bankswitch regions are present within D region.
       * 0 = 1 bank (only primary, no alternate)
       * 1 = 2 banks (primary and 1 alternate)
       * 2 = 4 banks (primary and 3 alternates)
       * others = reserved
       */
      scml2::bitfield< unsigned int > BANK_SW_D;
      unsigned int BANK_SW_D_reset_value;
      /**
       * Indicates size of RAM F/W storage:
       * 0 = No F/W RAM
       * 1 = 256 words
       * 2 = 512 words
       * 3 = 1024 words
       * 4 = 2048 words
       * 5 = 4096 words
       * others = reserved
       */
      scml2::bitfield< unsigned int > FW_RAM_SIZE;
      unsigned int FW_RAM_SIZE_reset_value;
      /**
       * Indicates size of ROM F/W storage:
       * 0 = No F/W ROM
       * 1 = 256 words
       * 2 = 512 words
       * 3 = 1024 words
       * 4 = 2048 words
       * 5 = 4096 words
       * others = reserved
       */
      scml2::bitfield< unsigned int > FW_ROM_SIZE;
      unsigned int FW_ROM_SIZE_reset_value;
      /**
       * Indicates maximum ECC operation size which may be performed:
       * 0 = ECC ops not supported
       * 1 = 256 bits
       * 2 = 512 bits
       * 3 = 1024 bits
       * others = reserved
       */
      scml2::bitfield< unsigned int > ECC_MAX;
      unsigned int ECC_MAX_reset_value;
      /**
       * Indicates maximum RSA operation size which may be performed:
       * 0 = RSA ops not supported
       * 1 = 512 bits
       * 2 = 1024 bits
       * 3 = 2048 bits
       * 4 = 4096 bits
       * others = reserved
       */
      scml2::bitfield< unsigned int > RSA_MAX;
      unsigned int RSA_MAX_reset_value;
      /**
       * Indicates width of the ALU:
       * 0 = 32 bits
       * 1 = 64 bits
       * 2 = 128 bits
       * 3 = reserved
       */
      scml2::bitfield< unsigned int > ALU_WIDTH;
      unsigned int ALU_WIDTH_reset_value;
      /**
       * Indicates TA/DPA countermeasure support:
       * 0 = No countermeasure support
       * 1 = TA support
       * 2 = DPA/TA support
       * 3 = reserved
       */
      scml2::bitfield< unsigned int > TA_DPA_SUPPORT;
      unsigned int TA_DPA_SUPPORT_reset_value;
      /* Indicates a type-2 register format. Other types are reserved. */
      scml2::bitfield< unsigned int > FORMAT_TYPE;
      unsigned int FORMAT_TYPE_reset_value;
    };
    struct STACK_PNTR_type : public scml2::reg< unsigned int > {
      ~STACK_PNTR_type();
      STACK_PNTR_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Stack pointer */
      scml2::bitfield< unsigned int > PNTR;
      unsigned int PNTR_reset_value;
    };
    struct INSTR_SINCE_GO_type : public scml2::reg< unsigned int > {
      ~INSTR_SINCE_GO_type();
      INSTR_SINCE_GO_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Instructions executed since CTRL.GO asserted */
      scml2::bitfield< unsigned int > INSTRS;
      unsigned int INSTRS_reset_value;
    };
    struct CONFIG_type : public scml2::reg< unsigned int > {
      ~CONFIG_type();
      CONFIG_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Used to prevent the synthesizer from removing what would otherwise be a W/O memory.
       * This register bit is never set in normal modes of operation and not intended to be written by the end user.
       * This bit only exists in the optional DPA-hardened configurations and primarily exists for synthesis purposes.
       * Test use only!
       *    0 = Normal C-memory access
       *    1 = Switch the C-memory read/write-access slot to the DPA shadow memory.
       */
      scml2::bitfield< unsigned int > ALT_ACCESS;
      unsigned int ALT_ACCESS_reset_value;
      /**
       * Cause the data memory regions to be endian swapped on transfer in or out of core.
       * Setting this bit converts each 32-bit word to big-endian format. The location of the each byte within the word are reversed, however the word order remains the same as its little endian counterpart. Only data written or read by the host to/from blocks A, B, C, and D are affected by this register setting. Access to the control/status registers and firmware area are unaffected regardless of the bit's setting.
       *    0 = No Byte Lane Swap (little endian)
       *    1 = Byte Lane Swap (big endian)
       */
      scml2::bitfield< unsigned int > ENDIAN_SWAP;
      unsigned int ENDIAN_SWAP_reset_value;
    };
    struct STAT_type : public scml2::reg< unsigned int > {
      ~STAT_type();
      STAT_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * W1 = Acknowledge existing interrupt
       * W0 = NOP
       * R1 = Unacknowledged interrupt exists
       * R0 = No unacknowledged interrupt exists
       */
      scml2::bitfield< unsigned int > DONE;
      unsigned int DONE_reset_value;
    };
    struct FLAGS_type : public scml2::reg< unsigned int > {
      ~FLAGS_type();
      FLAGS_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Zero flag */
      scml2::bitfield< unsigned int > Z;
      unsigned int Z_reset_value;
      /* Memory-test flag */
      scml2::bitfield< unsigned int > M;
      unsigned int M_reset_value;
      /* Borrow flag */
      scml2::bitfield< unsigned int > B;
      unsigned int B_reset_value;
      /* Carry flag */
      scml2::bitfield< unsigned int > C;
      unsigned int C_reset_value;
      /* User flag 0 */
      scml2::bitfield< unsigned int > F0;
      unsigned int F0_reset_value;
      /* User flag 1 */
      scml2::bitfield< unsigned int > F1;
      unsigned int F1_reset_value;
      /* User flag 2 */
      scml2::bitfield< unsigned int > F2;
      unsigned int F2_reset_value;
      /* User flag 3 */
      scml2::bitfield< unsigned int > F3;
      unsigned int F3_reset_value;
    };
    struct WATCHDOG_type : public scml2::reg< unsigned int > {
      ~WATCHDOG_type();
      WATCHDOG_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Watchdog reload value */
      scml2::bitfield< unsigned int > RELOAD;
      unsigned int RELOAD_reset_value;
    };
    struct CYCLES_SINCE_GO_type : public scml2::reg< unsigned int > {
      ~CYCLES_SINCE_GO_type();
      CYCLES_SINCE_GO_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Clock cycles since CTRL.GO asserted */
      scml2::bitfield< unsigned int > CYCLES;
      unsigned int CYCLES_reset_value;
    };
    struct INDEX_I_type : public scml2::reg< unsigned int > {
      ~INDEX_I_type();
      INDEX_I_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Index register I */
      scml2::bitfield< unsigned int > INDEX;
      unsigned int INDEX_reset_value;
    };
    struct INDEX_J_type : public scml2::reg< unsigned int > {
      ~INDEX_J_type();
      INDEX_J_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Index register J */
      scml2::bitfield< unsigned int > INDEX;
      unsigned int INDEX_reset_value;
    };
    struct INDEX_K_type : public scml2::reg< unsigned int > {
      ~INDEX_K_type();
      INDEX_K_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Index register K */
      scml2::bitfield< unsigned int > INDEX;
      unsigned int INDEX_reset_value;
    };
    struct INDEX_L_type : public scml2::reg< unsigned int > {
      ~INDEX_L_type();
      INDEX_L_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Index register L */
      scml2::bitfield< unsigned int > INDEX;
      unsigned int INDEX_reset_value;
    };
    struct IRQ_EN_type : public scml2::reg< unsigned int > {
      ~IRQ_EN_type();
      IRQ_EN_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * 0 = Disable interrupt signal at O_irq
       * 1 = Enable STAT.IRQ to generate an interrupt signal at O_irq
       */
      scml2::bitfield< unsigned int > IE;
      unsigned int IE_reset_value;
    };
    struct JMP_PROB_type : public scml2::reg< unsigned int > {
      ~JMP_PROB_type();
      JMP_PROB_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * 13'h1FFF = 100% probability
       * 0 = 0% probability
       * Other = 100% * Other/13'h1FFF probability
       */
      scml2::bitfield< unsigned int > PROB;
      unsigned int PROB_reset_value;
    };
    struct JMP_PROB_LFSR_type : public scml2::reg< unsigned int > {
      ~JMP_PROB_LFSR_type();
      JMP_PROB_LFSR_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /**
       * Seed to write to probability LFSR.>br>The seed must be between 0 and 0x1FFF.
       * NOTE: A seed of 0 will automatically be detected by the PKA H/W and written as 1 to prevent LFSR lockup.
       */
      scml2::bitfield< unsigned int > INIT;
      unsigned int INIT_reset_value;
    };
    struct BANK_SW_A_type : public scml2::reg< unsigned int > {
      ~BANK_SW_A_type();
      BANK_SW_A_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Currently selected bank in region A */
      scml2::bitfield< unsigned int > BANK_SW;
    };
    struct BANK_SW_B_type : public scml2::reg< unsigned int > {
      ~BANK_SW_B_type();
      BANK_SW_B_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Currently selected bank in region B */
      scml2::bitfield< unsigned int > BANK_SW;
    };
    struct BANK_SW_C_type : public scml2::reg< unsigned int > {
      ~BANK_SW_C_type();
      BANK_SW_C_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Currently selected bank in region C */
      scml2::bitfield< unsigned int > BANK_SW;
    };
    struct BANK_SW_D_type : public scml2::reg< unsigned int > {
      ~BANK_SW_D_type();
      BANK_SW_D_type(const std::string& name, scml2::toplevel_memory_base& memory, unsigned long long offset);
      using scml2::reg< unsigned int >::operator=;
      using scml2::reg< unsigned int >::operator+=;
      using scml2::reg< unsigned int >::operator-=;
      using scml2::reg< unsigned int >::operator/=;
      using scml2::reg< unsigned int >::operator*=;
      using scml2::reg< unsigned int >::operator%=;
      using scml2::reg< unsigned int >::operator^=;
      using scml2::reg< unsigned int >::operator&=;
      using scml2::reg< unsigned int >::operator|=;
      using scml2::reg< unsigned int >::operator>>=;
      using scml2::reg< unsigned int >::operator<<=;
      using scml2::reg< unsigned int >::operator--;
      using scml2::reg< unsigned int >::operator++;
      using scml2::reg< unsigned int >::operator unsigned int;
      /* Currently selected bank in region D */
      scml2::bitfield< unsigned int > BANK_SW;
    };
    scml2::objects::router<unsigned int> p_mem_socket_router;
    scml2::reset_slave_engine p_reset_protocol_engine;
    scml2::interrupt_master_engine p_intr_protocol_engine;
    scml2::tlm2_ft_target_port_pe<32> p_mem_socket_protocol_engine;
    /* Control/Configuration/Status registers */
    scml2::memory< unsigned int > sep_pka;
    /* Operand memory region A. This region holds the virtual large data registers for region A. This region starts at location 0x400. The length of this region is configuration dependent and may be sparsely populated. The region-s maximum length is 0x400 bytes. */
    scml2::memory_alias<unsigned int> bank_A;
    /* Operand memory region B. This region holds the virtual large data registers for region B. This region starts at location 0x800. The length of this region is configuration dependent and may be sparsely populated. The region-s maximum length is 0x400 bytes. */
    scml2::memory_alias<unsigned int> bank_B;
    /* Operand memory region C. This region holds the virtual large data registers for region C. This region starts at location 0xC00. The length of this region is configuration dependent and may be sparsely populated. The region-s maximum length is 0x400 bytes. */
    scml2::memory_alias<unsigned int> bank_C;
    /* Operand memory region D. This region holds the virtual large data registers for region D. This region starts at location 0x1000. The length of this region is configuration dependent and may be sparsely populated. The region-s maximum length is 0x800 bytes. */
    scml2::memory_alias<unsigned int> bank_D;
    /* F/W memory area This region is configuration dependent If the configuration has been selected for pure RAM or pure ROM, this region is devoted to that single memory type. If the configuration has been selected for mixed RAM/ROM, then the RAM will be at the base of this region (0x4000) and the ROM start halfway up the region at 0x6000. The length of the areas in this region are configuration dependent and may be sparsely populated. */
    scml2::memory_alias<unsigned int> pka_fw;
    /* Main Control register */
    CTRL_type CTRL;
    /* Program entry point */
    ENTRY_PNT_type ENTRY_PNT;
    /* Program return code register */
    RTN_CODE_type RTN_CODE;
    /**
     * Contains the static build-time parameter enumerations.
     * Current cores use a type-2 format.
     */
    BUILD_CONFIG_type BUILD_CONFIG;
    /**
     * Indicates depth of the function stack at any point in time.
     * The stack pointer can be cleared by writing a 0 to this register while the PKA is stopped.
     * Writes at any other time will be ignored by the PKA.
     * This register should be set to 0 prior to starting new PKA operations.
     */
    STACK_PNTR_type STACK_PNTR;
    /**
     * Indicates number of instructions the PKA has executed since the last CTRL.GO was issued.
     * This register is automatically cleared at the next assertion of CTRL.GO and retains its value after the execution has stopped.
     */
    INSTR_SINCE_GO_type INSTR_SINCE_GO;
    /* Sets configuration state within the PKA. */
    CONFIG_type CONFIG;
    /**
     * Used to monitor and acknowledge PKA interrupts.
     * The STAT.IRQ bit is used indicate when the PKA has completed its task. If the CTRL.IRQ_EN bit is set, the STAT.IRQ bit will generate an external interrupt while it is asserted by the underlying engine.
     * The STAT.IRQ bit will remain asserted until the STAT.IRQ bit is written with a 1.
     */
    STAT_type STAT;
    /**
     * Indicates the settings of the PKA's flags at any point in time.
     * Any of the flags can be set or cleared by writing a 1 or 0 to the appropriate bit in this register while the PKA is stopped. Writes at any other time will be ignored by the PKA.
     * The User Flags can be examined, set or cleared under firmware control for various firmware-defined purposes. They can also be read or written by host S/W. From usage point-of-view, they are typically used to select various features or options in certain F/W loads.
     * This register should be set to 0 prior to starting new PKA operations unless a function specifically requests otherwise. Consult the F/W documentation for more details.
     */
    FLAGS_type FLAGS;
    /**
     * Used to provide a bounded limit on the number of instructions the PKA will execute after the assertion of CTRL.GO.
     * This register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA.
     * When a CTRL.GO is issued, the WATCHDOG register value is copied into a down-counter which is decremented every time an instruction is executed. If this down-counter reaches a value of 0, the watchdog will stop the PKA and issue a RTN_CODE.STOP_REASON value of 4.
     * HINT: In order to set this to a meaningful value, the user must have an idea how long an operation might take under normal circumstances. The INSTR_SINCE_GO register can be used to get an indication of a normal value for this register. A suggestion might be to take the longest INSTR_SINCE_GO value seen for a particular function/operation and triple or quadruple the number before writing it to the watchdog register for that class of operation.
     */
    WATCHDOG_type WATCHDOG;
    /**
     * Indicates the number of clock cycles that have elapsed since the last CTRL.GO was issued.
     * This register is cleared on assertion of CTRL.GO and holds its value after the execution has stopped.
     * This register can be used to profile specific programs with the PKA.
     */
    CYCLES_SINCE_GO_type CYCLES_SINCE_GO;
    /**
     * Indicates the value of the I index register at any point in time.
     * The index register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA.
     * This register should be set to 0 prior to starting new PKA operations unless a function specifically requests otherwise. Consult the F/W documentation for more details.
     */
    INDEX_I_type INDEX_I;
    /**
     * Indicates the value of the J index register at any point in time.
     * The index register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA.
     * This register should be set to 0 prior to starting new PKA operations unless a function specifically requests otherwise. Consult the F/W documentation for more details.
     */
    INDEX_J_type INDEX_J;
    /**
     * Indicates the value of the K index register at any point in time.
     * The index register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA.
     * This register should be set to 0 prior to starting new PKA operations unless a function specifically requests otherwise. Consult the F/W documentation for more details.
     */
    INDEX_K_type INDEX_K;
    /**
     * Indicates the value of the L index register at any point in time.
     * The index register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA.
     * This register should be set to 0 prior to starting new PKA operations unless a function specifically requests otherwise. Consult the F/W documentation for more details.
     */
    INDEX_L_type INDEX_L;
    /**
     * Gates the STAT.IRQ signal to the core's external interrupt port.
     * NOTE: The IRQ_EN bit acts directly to gate the STAT.IRQ bit to the O_irq port. The O_irq signal can be removed by clearing the IRQ_EN signal, however this will leave the STAT.IRQ bit still active. The correct method for clearing an interrupt is to write a 1 to the STAT.IRQ bit in the STAT register.
     */
    IRQ_EN_type IRQ_EN;
    /**
     * Sets probability of executing a dummy operation in the Modexp and/or PMult operation.
     * If JMP_PROB register is set to 0 (0%), no dummy operations will be performed during Modexp and/or PMult and the PKA will execute the function at its highest possible performance. However, the fact that there is different execution timing between a 0 and 1 bit in the exponent and/or key can be detected by Timing Analysis attacks (TA) and/or Simple Power Analysis attacks (SPA).
     * If JMP_PROB is set to 0x1FFF (100%), every 0 bit in the exponent (for Modexp) and/or key (for PMult) will result in a dummy operation, indistinguishable from the processing of a 1 bit. This obviously slows the performance of the engine for these functions but provides protection against timing side-channel attacks.
     * The JMP_PROB register can be used to dial in any amount of TA/SPA protection by setting the value to between 0 and 0x1FFF. For example, if the register is set to 0x7FF (i.e.: 0x1FFF/4), this will result in, on average, 25% of the 0 bits executing a dummy operation and 75% skipping the dummy operation. Which F/W execution path is chosen by the PKA is a function of the internal random number generator and cannot be predicted externally. The approximate performance penalty associated with dialing up non-0 values can be scaled by the percentage of TA/SPA resistance dialed in with this register and the rule-of-thumb.
     */
    JMP_PROB_type JMP_PROB;
    /**
     * The JMP_PROB_LFSR probability LFSR seed register is used to initialize the probability LFSR.
     * The LFSR will be re-initialized to this value unless the host attempts to write a 0. In case a 0 is written by the host, internal logic will trap the 0 and substitute a value of 1. (LFSRs enter lock-up if set to 0.)
     * This register is not normally written by the host S/W.
     */
    JMP_PROB_LFSR_type JMP_PROB_LFSR;
    /* Shows and/or sets value of the A bank switches. The bank switch register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA. Bank switching allows the PKA to have access to more internal data storage, though not all accessible at one time. The data is accessed by the F/W switching to that bank while the program is running (via op-codes) and/or when the host is writing or reading data (via this register). This register is configuration dependent. If the configuration does not include this feature, the bits will be R/O and will show up as 0 when read. Architecturally, configurations can support 0 (i.e.: no bank switching), 1, or 2 bits of bank switching, allowing for 1, 2 or 4 banks of data for any of the register memories. However, this feature is only required for specific F/W programs. At this time, only Weierstrass ECC Shamir and Weierstrass ECC-521 Shamir require A to support bank switching. NOTE: The F/W should set this register to 0, if present, prior to loading new data! */
    BANK_SW_A_type BANK_SW_A;
    unsigned int BANK_SW_A_reset_value;
    unsigned int BANK_SW_A_reset_mask;
    /* Shows and/or sets value of the B bank switches. The bank switch register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA. Bank switching allows the PKA to have access to more internal data storage, though not all accessible at one time. The data is accessed by the F/W switching to that bank while the program is running (via op-codes) and/or when the host is writing or reading data (via this register). This register is configuration dependent. If the configuration does not include this feature, the bits will be R/O and will show up as 0 when read. Architecturally, configurations can support 0 (i.e.: no bank switching), 1, or 2 bits of bank switching, allowing for 1, 2 or 4 banks of data for any of the register memories. However, this feature is only required for specific F/W programs. At this time, no F/W components require region B to support bank switching. NOTE: The F/W should set this register to 0, if present, prior to loading new data! */
    BANK_SW_B_type BANK_SW_B;
    unsigned int BANK_SW_B_reset_value;
    unsigned int BANK_SW_B_reset_mask;
    /* Shows and/or sets value of the C bank switches. The bank switch register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA. Bank switching allows the PKA to have access to more internal data storage, though not all accessible at one time. The data is accessed by the F/W switching to that bank while the program is running (via op-codes) and/or when the host is writing or reading data (via this register). This register is configuration dependent. If the configuration does not include this feature, the bits will be R/O and will show up as 0 when read. Architecturally, configurations can support 0 (i.e.: no bank switching), 1, or 2 bits of bank switching, allowing for 1, 2 or 4 banks of data for any of the register memories. However, this feature is only required for specific F/W programs. At this time, no F/W components require region B to support bank switching. NOTE: The F/W should set this register to 0, if present, prior to loading new data! */
    BANK_SW_C_type BANK_SW_C;
    unsigned int BANK_SW_C_reset_value;
    unsigned int BANK_SW_C_reset_mask;
    /* Shows and/or sets value of the D bank switches. The bank switch register can only be written while the PKA is stopped. Writes at any other time will be ignored by the PKA. Bank switching allows the PKA to have access to more internal data storage, though not all accessible at one time. The data is accessed by the F/W switching to that bank while the program is running (via op-codes) and/or when the host is writing or reading data (via this register). This register is configuration dependent. If the configuration does not include this feature, the bits will be R/O and will show up as 0 when read. Architecturally, configurations can support 0 (i.e.: no bank switching), 1, or 2 bits of bank switching, allowing for 1, 2 or 4 banks of data for any of the register memories. However, this feature is only required for specific F/W programs. At this time, no F/W components require region B to support bank switching. NOTE: The F/W should set this register to 0, if present, prior to loading new data! */
    BANK_SW_D_type BANK_SW_D;
    unsigned int BANK_SW_D_reset_value;
    unsigned int BANK_SW_D_reset_mask;
private:
};
#ifdef _WIN32
#pragma optimize("",on)
#else
#pragma GCC pop_options
#endif


}  // end of namespace mylibrary
