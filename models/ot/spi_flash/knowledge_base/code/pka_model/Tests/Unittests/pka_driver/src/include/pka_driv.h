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
 * Copyright (c) 2020,2022 Synopsys, Inc. and/or its affiliates.
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

#ifndef PKA_DRIV_H_
#define PKA_DRIV_H_

#include "pka_debug.h"

/**
 * \ingroup E_PKADriver
 *
 * These functions are platform specific and must be changed to support the target platform.
 * As delivered the porting layer is implemented as a user space Linux kernel driver.
 * Entropy is provided using the C function rand and should be replaced.
 *
 *
 * @{
 *
 */

/**
 * \details
 *  Reads and returns a 32-bit value from the supplied PKA register address, word aligned.
 *
 *
 * \param [in] offset   Offset of the register to read.
 *
 * \return
 *    Returns the 32 bit value
 */
 uint32_t pka_driv_read(uint32_t offset);

 /**
  * \details
  *  Reads data from a contiguous set of 32-bit registers into the supplied byte buffer
  *  This function converts from a LSB-first big integer to a MSB-first big integer while reading.
  *  If the read request is not a multiple of 4 bytes, the read will truncate the MSB end to the requested number of bytes.
  *
  *
  * \param [in] offset   Offset of the register to read the array of data from.
  * \param [in] buffer   Pointer to the buffer of data to read.
  * \param [in] length   Length of the data to read.
  *
  */
 void pka_driv_read_bigint(uint32_t offset, uint8_t *buffer, uint32_t length);

 /**
  * \details
  *  Reads data from a contiguous set of 32-bit registers into the supplied byte buffer
  *  This function converts from a MSB-first big integer to a LSB-first big integer while reading.
  *  If the read request is not a multiple of 4 bytes, the read will truncate the LSB end to the requested number of bytes.
  *
  *
  * \param [in] offset   Offset of the register to read the array of data from.
  * \param [in] buffer   Pointer to the buffer of data to read.
  * \param [in] length   Length of the data to read.
  *
  *
  */
 void pka_driv_read_leint(uint32_t offset, uint8_t *buffer, uint32_t length);

/**
 * \details
 *  Writes a supplied value to the supplied PKA register address, word aligned transfers only.
 *
 * \param [in] offset   Offset of the register to write.
 * \param [in] data     Data to write.
 *
 *
 */
void pka_driv_write(uint32_t offset, uint32_t data);

/**
 * \details
 *  Reads data from a contiguous set of 32-bit registers into the supplied byte buffer
 *  This function converts from a LSB-first big integer to a MSB-first big integer while reading.
 *  If the read request is not a multiple of 4 bytes, the read will truncate the MSB end to the requested number of bytes.
 *
 *
 * \param [in] offset   Offset of the register to read the array of data from.
 * \param [in] buffer   Pointer to the buffer of data to write.
 * \param [in] length   Length of the data to write.
 *
 */
void pka_driv_write_bigint(uint32_t offset, const uint8_t *buffer, uint32_t length);

/**
 * \details
 *  Reads data from a contiguous set of 32-bit registers into the supplied byte buffer
 *  This function converts from a MSB-first big integer to a LSB-first big integer while reading.
 *  If the read request is not a multiple of 4 bytes, the read will truncate the LSB end to the requested number of bytes.
 *
 *
 * \param [in] offset   Offset of the register to read the array of data from.
 * \param [in] buffer   Pointer to the buffer of data to write.
 * \param [in] length   Length of the data to write.
 *
 */
void pka_driv_write_leint(uint32_t offset, uint8_t *buffer, uint32_t length);


/**
 * \details
 * \param [in] offset    Offset of the register to poll.
 * \param [in] bitmask   Mask to indicate which bits to monitor.
 *
 * \return
 *    - #PKA_OK      Operation completed successfully.
 *    - #PKA_ERR     Operation completed with an error.
 *    - #PKA_TIMEOUT Operation did not complete and timed out.
 */
int pka_driv_wait(uint32_t offset, uint32_t bitmask);

/**
 * \details
 *  This function locks access to the hardware.
 *  This function along with pka_driv_unlock is used for multiprocess software implementation
 *  Not required for single process or single thread software implementations.
 *
 * \return
 *    - #PKA_OK      Operation completed successfully.
 *    - #PKA_ERR     Operation completed with an error.
 *
 */
int pka_driv_lock(void);

/**
 * \details
 *  This function unlocks access to the hardware.
 *  This function along with pka_driv_lock is used for multiprocess software implementation
 *  Not required for single process or single thread software implementations.
 *
 * \return
 *    - #PKA_OK      Operation completed successfully.
 *    - #PKA_ERR     Operation completed with an error.
 *
 */
int pka_driv_unlock(void);

/**
 * \details
 *  This function initialize the driver.
 *
 * \param [in] pka_address    Physical PKA address.
 * \return
 *    - #PKA_OK      Operation completed successfully.
 *    - #PKA_ERR     Operation completed with an error.
 */

int pka_driv_init(uint32_t *pka_address);

/**
 * \details
 *  This function closes  the driver.
 *
 * \param [in] pka_address    Physical PKA address.
 * \return
 *    - #PKA_OK      Operation completed successfully.
 *    - #PKA_ERR     Operation completed with an error.
 */

int pka_driv_close(uint32_t *pka_address);

/**
 * \details
 *  This function generates random data for the driver. It must be
 *  updated with a cryptographically sound random number generator.
 *
 * \param [in] data    Pointer to buffer to hold the random data.
 * \param [in] size    Size in bytes.
 *
 * \return
 *    - #PKA_OK      Operation completed successfully.
 */

int pka_driv_random(uint8_t *data, uint32_t size);
/**
 * @}
 */
#endif
