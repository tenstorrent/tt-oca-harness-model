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

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ESM_MAX_FILE_NAME          256

static const char usage_msg1[] =
"\n HEX2H TOOL \n"\
"\n Converts hex file to .h file  \n"\
"Usage: %s [-h?] --in ... --out ...\n"\
" \n"\
" ----------------------------------------------------------------------------------------------------\n"\
" -i  --in      file  :MUST specify INPUT, a hex file which is the PKA's firmware file, (e.g: clp300_ram_fw.hex) \n"\
" -o  --out     file  :MUST specify OUTPUT, .h file (e.g: ram_fw.h) \n"\
" ----------------------------------------------------------------------------------------------------\n"\
" -h  --help\n";


int main(int argc, char **argv)
{
   FILE *fptr, *fpo;;
   int cnt0=0;
   int cnt1=0;
   int i=0;
   int cols = 12;
   int file=0;
   int32_t c;
   unsigned char *work_buf;
   unsigned char *out_buf;
   struct stat fileStat;
   unsigned char input_config_file[ESM_MAX_FILE_NAME];
   unsigned char output_config_file[ESM_MAX_FILE_NAME];
   char*   ip_file;
   char*   op_file;
   ip_file = 0;
   op_file = 0;


   while ((c = getopt(argc, argv, "?hi:o:"))!= -1)
   {
       switch (c)
       {
       case 'h':
       case '?':
           fprintf(stderr, usage_msg1, argv[0]);
           break;
       case 'i':
           strcpy(input_config_file,optarg);
           ip_file = &input_config_file[0];
           break;
       case 'o':
           strcpy(output_config_file,optarg);
           op_file = &output_config_file[0];
           break;
       default:
          printf("Bad input command %c\n", c);
          return -1;
          break;
        }
   }

   if (ip_file == 0)
   {
       fprintf(stderr, "Error: Must supply an INPUT file\n");
       return -1;
   }

   if (op_file == 0)
   {
       fprintf(stderr, "Error: Must supply an OUTPUT file\n");
       return -1;
   }

   file = open(ip_file,O_RDONLY);
   if (file == -1)
   {
      fprintf(stderr," failed to open %s file: %s.\n", ip_file, strerror(errno));
      return -1;
   }

   // Check file size
   fstat(file,&fileStat);
   fprintf(stderr,"Reading  %ld bytes image from '%s' file\n", (long)fileStat.st_size, ip_file);

   // Allocate firmware buffer
   work_buf = malloc(fileStat.st_size);
   if (!work_buf)
   {
      fprintf(stderr,"failed to allocate %ld bytes to load %s file.\n", (long)fileStat.st_size, ip_file);
      return -1;
   }

   out_buf = malloc(fileStat.st_size);
  if (!out_buf)
  {
      fprintf(stderr,"failed to allocate %ld bytes for out_buf.\n", (long)fileStat.st_size);
      return -1;
  }

  // Read firmware from the file into the memory buffer
  if (read(file,work_buf,fileStat.st_size) != fileStat.st_size)
  {
      fprintf(stderr,"failed to read %ld bytes to load %s file.\n", (long)fileStat.st_size, ip_file);
      free(work_buf);
      return -1;
  }

   if (work_buf[0] != 0x2f  || work_buf[1] != 0x2f )
   {
       fprintf(stderr," file %s has Invalid format  .\n",  ip_file );
       return -1;
   }

   for (cnt0 =0; cnt0 <fileStat.st_size ; cnt0++)
   {
       // Find the location of first data
       if (work_buf [cnt0] == 0x0a &&  work_buf [cnt0 +1] != 0x2f)
       {
           break;
       }
   }

   // change from string to hex
   for(; cnt0 <fileStat.st_size ;cnt0++)
   {
        if (work_buf [cnt0] !=  0x0a )
        {
            if (work_buf [cnt0] >= '0' && work_buf [cnt0] <= '9')
            {
                out_buf[cnt1] = (work_buf[cnt0] - '0');
            }
            else
            {
                out_buf[cnt1] = (work_buf[cnt0] - 'a') + 10;
            }
            cnt1++;
      }
   }

  for (i=0; i< (cnt1/2) ;i++)
  {
      out_buf [i] = (out_buf [i*2] << 4) + out_buf [(i*2) +1];
  }


   // write the outpu buffer to the output file
  if ((fpo = fopen(op_file, "wb")) == NULL)
  {
      printf("Unable to open OUTPUT file %s\n", op_file);
      return -13;
  }


  fprintf(fpo,"unsigned char ram_fw_bin[] = {\n");

  for (i=0; i < cnt1/2 ;i++ )
  {
      fprintf(fpo, " 0x%02x" , out_buf[i]);
      // last entry should be without comma
      if (i != (cnt1/2 - 1))
      {
          fprintf(fpo, ",");
      }

      if( (i+1) % cols == 0)
      {
          fprintf(fpo, " \n");
      }
  }

  if( i % cols != 0)
  {
      fprintf(fpo, " \n");
  }

  fprintf(fpo,"};\n");
  fprintf(fpo,"unsigned int ram_fw_bin_len = ");
  fprintf(fpo, "%d; \n", cnt1/2 );

  close(file);
  fclose(fpo);
  free(work_buf);
  free(out_buf);

   return 0;
}


