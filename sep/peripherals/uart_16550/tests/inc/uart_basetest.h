
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class UART_basetest : public sc_module
{
  public:
    tlm_utils::simple_initiator_socket<UART_basetest, 32> initiator_socket;
    enum Register_offset
    {
      RBR_OFFSET = (0x00 + 0x00), 
      THR_OFFSET = (0x00 + 0x00), 
      IER_OFFSET = (0x04 + 0x00), 
      IIR_OFFSET = (0x08 + 0x00), 
      FCR_OFFSET = (0x08 + 0x00), 
      LCR_OFFSET = (0x0C + 0x00), 
      LSR_OFFSET = (0x14 + 0x00), 
      DLL_OFFSET = (0x00 + 0x00), 
      DLM_OFFSET = (0x04 + 0x00), 
      MCR_OFFSET = (0x10 + 0x00), 
      MSR_OFFSET = (0x18 + 0x00), 
      SCR_OFFSET = (0x1C + 0x00), 
      ECR_OFFSET = (0x20 + 0x00), 
      ITR_OFFSET = (0x24 + 0x00)  
    };

    enum Register_Read_Access
    {
      RBR_READ = (0xff), 
      THR_READ = (0x0), 
      IER_READ = (0xff), 
      IIR_READ = (0xff), 
      FCR_READ = (0x0), 
      LCR_READ = (0xff), 
      LSR_READ = (0xff), 
      DLL_READ = (0xff), 
      DLM_READ = (0xff), 
      MCR_READ = (0xff), 
      MSR_READ = (0xff), 
      SCR_READ = (0xff), 
      ECR_READ = (0xff), 
      ITR_READ = (0xff)  
    };

    enum Register_Write_Access
    {
      RBR_WRITE = (0x0), 
      THR_WRITE = (0xff), 
      IER_WRITE = (0xff), 
      IIR_WRITE = (0x0), 
      FCR_WRITE = (0xff), 
      LCR_WRITE = (0xff), 
      LSR_WRITE = (0x0), 
      DLL_WRITE = (0xff), 
      DLM_WRITE = (0xff), 
      MCR_WRITE = (0xff), 
      MSR_WRITE = (0x0), 
      SCR_WRITE = (0xff), 
      ECR_WRITE = (0xff), 
      ITR_WRITE = (0xff)
    };

    enum Register_Reset_Val
    {
      RBR_RESET = (0x0), 
      THR_RESET = (0x0), 
      IER_RESET = (0x0), 
      IIR_RESET = (0x01), 
      FCR_RESET = (0x0), 
      LCR_RESET = (0x0), 
      LSR_RESET = (0x60), 
      DLL_RESET = (0x0), 
      DLM_RESET = (0x0), 
      MCR_RESET = (0x00), 
      MSR_RESET = (0x00), 
      SCR_RESET = (0x00), 
      ECR_RESET = (0x00), 
      ITR_RESET = (0x00)
    };
     
    struct Register_Property_t
    {
		  unsigned int reg_offset;
		  unsigned int read_mask;
		  unsigned int write_mask;
		  unsigned int reg_reset;
		  std::string reg_name;
    };

    UART_basetest(sc_module_name name) : sc_module(name)
    {

    }

};