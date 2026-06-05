#include "uart_base.h"

void UART_base::reset_all_registers()
{
  RBR.reset();
  THR.reset();
  IER.reset();
  IIR.reset();
  FCR.reset();
  LCR.reset();
  LSR.reset();
  DLL.reset();
  DLM.reset();
  MCR.reset();
  MSR.reset();
  SCR.reset();
  ECR.reset();
  ITR.reset();
}