#pragma once
#include "csml_logger.h"

extern CsmlLogger g_uart_logger;

#ifndef UART_TRACE
#define UART_TRACE(msg) CSML_DEBUG(3, g_uart_logger) << "UART: " << msg
#endif
#ifndef UART_DEBUG
#define UART_DEBUG(msg) CSML_DEBUG(3, g_uart_logger) << "UART: " << msg
#endif
#ifndef UART_INFO
#define UART_INFO(msg)  CSML_INFO(2,  g_uart_logger) << "UART: " << msg
#endif
#ifndef UART_WARN
#define UART_WARN(msg)  CSML_WARN(1,  g_uart_logger) << "UART: " << msg
#endif
#ifndef UART_ERROR
#define UART_ERROR(msg) CSML_ERROR(0, g_uart_logger) << "UART: " << msg
#endif
