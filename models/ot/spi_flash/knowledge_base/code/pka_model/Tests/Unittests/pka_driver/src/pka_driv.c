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

/*
* This is an example driver for the PKA which can support either Linux
* or bare-metal. As shown below the driver is implemented as a
* user-space Linux driver - the PKA register space is mmapped and the
* returned virtual pointer is used to reference the PKA registers.
*
* For bare-metal the implementation is identical which only a few
* small changes as there is no need to memmap because the (physical) address
* passed in can be used directly. See the relevant comments in the code
* to adapt to a bare-metal implementation.
*
*
*  Remove Linux specific includes
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if 0
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#endif

#include "pka.h"
#include "pka_driv.h"
#include "data_macros.h"
#include "../../pka_reg_access.h"

//
// Not needed for bare-metal
//
#define PKA_HW_MMAP_SIZE 0x8000

//
// Only needed if more than one thread/process
// needs access to PKA. Required for atomic use of PKA
// operations.
//
//static pthread_mutex_t hardware_lock;
//static pthread_mutexattr_t attr;


//
// Internal file descriptor used for linux mmap and munmap
// Comment out if using bare metal.
//
int fd_pka = -1;

//
// Comment out this function if using bare-metal
//
#if 0
uint32_t linux_driv_init(uint32_t baseAddr, int *fd)
{
    void *map_base, *virt_addr;
    unsigned page_size, mapped_size, offset_in_page;
    unsigned width = 8 * sizeof(int);

    mapped_size = PKA_HW_MMAP_SIZE;
    width = 32;
    //PKA_REPORT_ERR_HEX("baseAddr", baseAddr);
    //PKA_REPORT_ERR_HEX("mapped_size", mapped_size);

    *fd = open("/dev/mem", (O_RDWR | O_SYNC) );
    if (*fd < 0)
    {
       PKA_REPORT("Cannot open /dev/mem\n");
       return (uint32_t)NULL;
    }

    page_size = getpagesize();
    offset_in_page = (unsigned)baseAddr & (page_size - 1);
    //PKA_REPORT_ERR("offset_in_page", offset_in_page);

    if (offset_in_page + width > page_size) {
        /* This access spans pages.
         * Must map two pages to make it possible: */
        mapped_size *= 2;
    }

    map_base = mmap(NULL,
                    mapped_size,
                    (PROT_READ | PROT_WRITE),
                    MAP_SHARED,
                    *fd,
                    baseAddr & ~(off_t)(page_size - 1));

    if (map_base == MAP_FAILED)
    {
       PKA_REPORT("map failed");
       return (uint32_t)NULL;
    }

    virt_addr = (char*)map_base + offset_in_page;
    //PKA_REPORT_ERR_HEX("virt_addr", virt_addr);

    return (uint32_t)virt_addr;
}
#endif


/**
 * Initialize the driver. For Linux you need to use a virtual pointer for
 * the PKA physical address passed in. In bare metal the address
 * passed in needs no such conversion.
 */
int  pka_driv_init(uint32_t *pka_address)
{
    (void)pka_address;
 #if 0
   //
   // This code block is specific to Linux to provide the physical to
   // virtual mapping. If you are using bare-metal then you
   // comment out or delete the following 5 lines of code
   //
   *pka_address = linux_driv_init(*pka_address, &fd_pka);
   if (*pka_address == (uint32_t)NULL)
   {
      return PKA_INVPARAM;
   }
#endif
   //
   // Exclusive access to the PKA is not required
   // in a single-threaded or bare-metal environment
   //
//   pthread_mutex_init(&hardware_lock, &attr);

#if 0
   //
   // This is only to initialize the C library rand function
   // and MUST NOT be used for deployment
   //
   srand((unsigned int)time(NULL));
#endif
   return PKA_OK;
}


/**
 *  Cleans up driver and closes it. This is Linux specific
 *  as you need to unmap the peripheral. A bare-metal
 *  implementation would only have the following line of
 *  code:
 *            return PKA_OK;
 */
int pka_driv_close(uint32_t *pka_address)
{
    (void)pka_address;
#if 0
   int err = 0;

   err = munmap(pka_address, PKA_HW_MMAP_SIZE);
   if (err == -1)
   {
      PKA_REPORT("Error unmapping pka");
   }
   close(fd_pka);

   if (err == -1)
   {
      return  PKA_INVPARAM;
   }
#endif
   return PKA_OK;
}

/**
 *  Reads a single 32 bit value from the specified accelerator.
 */
uint32_t pka_driv_read(uint32_t offset)
{
   return pka_reg_read(offset);
}

/**
*  Reads data from a contiguous set of 32-bit registers into the supplied byte buffer
*/
void pka_driv_read_bigint(uint32_t offset, uint8_t *buffer, uint32_t length)
{
    pka_reg_read_bigint(offset, buffer, length);
}


/**
 *  Writes a single 32 bit value from the specified address.
 */
void pka_driv_write(uint32_t offset, uint32_t data)
{
   pka_reg_write(offset, data);
}

/**
 *  Writes a buffer of data to the specified accelerator.
 */
void pka_driv_write_bigint(uint32_t offset, const uint8_t *buffer, uint32_t length)
{
    pka_reg_write_bigint(offset, buffer, length);
}


/**
 *  Polls the PKA waiting for the operation to complete. This is a
 *  basic blocking implementation to demonstrate waiting for completion. If
 *  you do not want a blocking operation then you could implement the following
 *  to support suspension in a threaded environment:
 *  1. Create a semaphore in the pka_drv_init function
 *  2. Register a PKA interrupt handler in your environment. In this handler you
 *     will free the semaphore when you receive the PKA interrupt.
 *  3. Modify this function to acquire the semaphore. This will cause the running
 *     thread to suspend until it is freed by the interrupt handler releasing the semaphore.
 *  4. Don't forget to delete the semaphore when exiting the driver (pka_driv_close)
 *
 *  If you are running bare-metal and want to do other work then you can put a function
 *  in the while loop to call out to some other code to run while polling the PKA for
 *  completion.
 *
 */
int pka_driv_wait(uint32_t offset, uint32_t bitmask)
{
   int max_retries = 0;
   uint32_t v;

   while (max_retries < PKA_MAX_TRIES_ON_WAIT) {
        v = pka_reg_read(offset);
        if ((bitmask & v) != 0)
            return PKA_OK;
        max_retries++;
   }
   PKA_REPORT("pka_driv_wait timeout");
   return PKA_TIMEOUT;
}


/*
 *  This function locks access to the hardware. Implementation
 *  only required in a multi-threaded environment where more than
 *  one thread/process is using the PKA. The implementation here
 *  shows a single process or bare-metal implementation.
 */
int pka_driv_lock(void)
{
   // Linux use.
   // If another thread has the lock then this thread will be
   // suspended until it is unlocked.
   //
   // commented out
   // uncomment if you need to use it
   return PKA_OK;
   /*
   int err = PKA_ERR;
   err = pthread_mutex_lock(&hardware_lock);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("lock fail", err);
   }
   return err;
   */
}


/*
 *  Paired function to the above, unlocks access to the hardware.
 */
int pka_driv_unlock(void)
{
   // commented out
   // uncomment if you need to use it
   return PKA_OK;
   /*
   int err = PKA_ERR;
   err = pthread_mutex_unlock(&hardware_lock);
   if (err != PKA_OK)
   {
      PKA_REPORT_ERR("unlock fail", err);
   }
   return err;
   */
}

/*
 * This function generates basic random data using the C library
 * rand for demonstration purposes and MUST NOT be deployed outside
 * of a test environment. For deployment a cryptographically
 * sound random number generator must be used.
 */
int pka_driv_random(uint8_t *data, uint32_t size)
{
   uint32_t i;

   for (i = 0; i < size; i++)
   {
      data[i] = (unsigned char) rand();
   }

   return PKA_OK;
}


