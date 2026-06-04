#pragma once
#include "uart_register.h"
#include <string.h>

class UART_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    UART_base(sc_module_name name, unsigned int memory_size) : sc_module(name), memory(std::string(name) + ".Memory", memory_size/sizeof(unsigned int)),
       RBR(std::string(name) + ".RBR", memory, (0x00 + 0x00)/sizeof(unsigned int)), 
       THR(std::string(name) + ".THR", memory, (0x00 + 0x00)/sizeof(unsigned int)), 
       IER(std::string(name) + ".IER", memory, (0x04 + 0x00)/sizeof(unsigned int)), 
       IIR(std::string(name) + ".IIR", memory, (0x08 + 0x00)/sizeof(unsigned int)), 
       FCR(std::string(name) + ".FCR", memory, (0x08 + 0x00)/sizeof(unsigned int)), 
       LCR(std::string(name) + ".LCR", memory, (0x0C + 0x00)/sizeof(unsigned int)), 
       LSR(std::string(name) + ".LSR", memory, (0x14 + 0x00)/sizeof(unsigned int)), 
       DLL(std::string(name) + ".DLL", memory, (0x00 + 0x00)/sizeof(unsigned int)), 
       DLM(std::string(name) + ".DLM", memory, (0x04 + 0x00)/sizeof(unsigned int)), 
       MCR(std::string(name) + ".MCR", memory, (0x10 + 0x00)/sizeof(unsigned int)), 
       MSR(std::string(name) + ".MSR", memory, (0x18 + 0x00)/sizeof(unsigned int)), 
       SCR(std::string(name) + ".SCR", memory, (0x1C + 0x00)/sizeof(unsigned int)), 
       ECR(std::string(name) + ".ECR", memory, (0x20 + 0x00)/sizeof(unsigned int)), 
       ITR(std::string(name) + ".ITR", memory, (0x24 + 0x00)/sizeof(unsigned int))
       {
         memory.bind_to_socket(target_socket);
       }

      csml_memory<32> memory;
      tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

      
      UART::RBR_type<32> RBR;
      
      UART::THR_type<32> THR;
      
      UART::IER_type<32> IER;
      
      UART::IIR_type<32> IIR;
      
      UART::FCR_type<32> FCR;
      
      UART::LCR_type<32> LCR;
      
      UART::LSR_type<32> LSR;
      
      UART::DLL_type<32> DLL;
      
      UART::DLM_type<32> DLM;
      
      UART::MCR_type<32> MCR;
      
      UART::MSR_type<32> MSR;
      
      UART::SCR_type<32> SCR;
      
      UART::ECR_type<32> ECR;
      
      UART::ITR_type<32> ITR;
      
      void reset_all_registers();
};
