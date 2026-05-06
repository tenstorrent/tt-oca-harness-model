/******************************************************************************
 * Copyright (c) 2021, Vayavya Labs Pvt. Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Vayavya Labs Pvt. Ltd. nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VAYAVYA LABS PVT. LTD. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/

#pragma once

#include <string>
#include <type_traits>
#include <systemc.h>


/**
 * @brief report mechanism for csml
 * The following report_type are allowed
 * INFO        // informative only
 * WARNING,    // indicates potentially incorrect condition
 * ERROR,      // indicates a definite problem
 * FATAL       // indicates a problem from which we cannot recover
 */

#define CSML_REPORT(report_type, msg_type, ...) SC_REPORT_##report_type(msg_type, form_report_string( __VA_ARGS__ ).c_str())

inline std::string form_report_string(std::string arg1)
{ 
  return arg1;
}

template<typename T>
inline std::string form_report_string(T arg)
{
  return std::to_string(arg);
}

inline std::string form_report_string( const char* arg)
{
  return arg ? std::string(arg) : std::string();
}

template<class... va_args>
std::string form_report_string(std::string arg1, va_args... args);

template<class T, class U>
inline std::string form_report_string(T arg1, U arg2)
{
   return form_report_string(arg1) + form_report_string(arg2);
}

template<class T, class... va_args>
inline std::string form_report_string(T arg1, va_args... args)
{
  return form_report_string(arg1) + form_report_string(args...);
}

template<class... va_args>
inline std::string form_report_string(std::string arg1, va_args... args)
{
  return arg1 + form_report_string(args...);
}