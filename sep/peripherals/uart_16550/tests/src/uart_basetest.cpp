#include "uart_basetest.h"

UART_basetest::Register_Property_t reg_map[14] = {
{UART_basetest::RBR_OFFSET, UART_basetest::RBR_READ, UART_basetest::RBR_WRITE, UART_basetest::RBR_RESET, "RBR"}, 
{UART_basetest::THR_OFFSET, UART_basetest::THR_READ, UART_basetest::THR_WRITE, UART_basetest::THR_RESET, "THR"}, 
{UART_basetest::IER_OFFSET, UART_basetest::IER_READ, UART_basetest::IER_WRITE, UART_basetest::IER_RESET, "IER"}, 
{UART_basetest::IIR_OFFSET, UART_basetest::IIR_READ, UART_basetest::IIR_WRITE, UART_basetest::IIR_RESET, "IIR"}, 
{UART_basetest::FCR_OFFSET, UART_basetest::FCR_READ, UART_basetest::FCR_WRITE, UART_basetest::FCR_RESET, "FCR"}, 
{UART_basetest::LCR_OFFSET, UART_basetest::LCR_READ, UART_basetest::LCR_WRITE, UART_basetest::LCR_RESET, "LCR"}, 
{UART_basetest::LSR_OFFSET, UART_basetest::LSR_READ, UART_basetest::LSR_WRITE, UART_basetest::LSR_RESET, "LSR"}, 
{UART_basetest::DLL_OFFSET, UART_basetest::DLL_READ, UART_basetest::DLL_WRITE, UART_basetest::DLL_RESET, "DLL"}, 
{UART_basetest::DLM_OFFSET, UART_basetest::DLM_READ, UART_basetest::DLM_WRITE, UART_basetest::DLM_RESET, "DLM"}, 
{UART_basetest::MCR_OFFSET, UART_basetest::MCR_READ, UART_basetest::MCR_WRITE, UART_basetest::MCR_RESET, "MCR"}, 
{UART_basetest::MSR_OFFSET, UART_basetest::MSR_READ, UART_basetest::MSR_WRITE, UART_basetest::MSR_RESET, "MSR"}, 
{UART_basetest::SCR_OFFSET, UART_basetest::SCR_READ, UART_basetest::SCR_WRITE, UART_basetest::SCR_RESET, "SCR"}, 
{UART_basetest::ECR_OFFSET, UART_basetest::ECR_READ, UART_basetest::ECR_WRITE, UART_basetest::ECR_RESET, "ECR"}, 
{UART_basetest::ITR_OFFSET, UART_basetest::ITR_READ, UART_basetest::ITR_WRITE, UART_basetest::ITR_RESET, "ITR"}};