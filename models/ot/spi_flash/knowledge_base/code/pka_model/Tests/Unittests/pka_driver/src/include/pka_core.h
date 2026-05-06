/*
 * This Synopsys software and associated documentation (hereinafter the
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you. The
 * Software IS NOT an item of Licensed Software or a Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Products
 * with Synopsys or any supplement thereto. Synopsys is a registered trademark
 * of Synopsys, Inc. Other names included in the SOFTWARE may be the
 * trademarks of their respective owners.
 *
 * The contents of this file are dual-licensed; you may select either version
 * 2 of the GNU General Public License ("GPL") or the BSD-3-Clause license
 * ("BSD-3-Clause"). The GPL is included in the COPYING file accompanying the
 * SOFTWARE. The BSD License is copied below.
 *
 * BSD-3-Clause License:
 * Copyright (c) 2020 Synopsys, Inc. and/or its affiliates.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer, without
 *    modification.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of the above-listed copyright holders may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PKA_CORE_H_
#define PKA_CORE_H_

#include "pka_defines.h"
#include "pka_debug.h"
#include "pka_driv.h"


/**
* Initializes the configuration structures in the instance pointer for the PKA.
*
*  Returns an error on invalid input or if it is unable to read the hardware configuration.
*
* \param  [in] state  PKA instance pointer.
* \param  [in] pka_addr   Base address for PKA registers
* \param  [in] buffer     PKA firmware byte buffer.
* \param  [in] length     Length of PKA firmware.
* \return
*   - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
*/
int pka_core_init(struct pka_state *state, uint32_t pka_addr, uint8_t *buffer, uint16_t length);

/**
*   This function polls the status register of the specified device and then returns the result of a running job.
*   The PKA's STAT.DONE bit is polled via the pka_drv_wait() function and then returns the value from the 
*   PKA's RTN_CODE register STOP_REASON field.
*
* \param  [in] state   PKA instance pointer.
* \return
*    - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
*/
int pka_core_wait(struct pka_state *state);

/**
 *   This function begins the hardware operation by setting CTRL.GO.
 *
 * \param  [in] state   PKA instance pointer.
 * \param  [in] entry   firmware entry defined in clp300_ram_fw.h
 * \param  [in] size    size in bytes
 * \return
 *   - error_Type      Hardware or Software failures as defined in pka_defines.h error_Type.
 */
int pka_core_go(struct pka_state *state, uint32_t entry, uint16_t size);



#endif
