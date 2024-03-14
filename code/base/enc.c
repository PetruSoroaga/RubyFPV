/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "enc.h"

#ifdef HW_CAPABILITY_WFBOHD

#include <sodium.h>

static int s_iOpenIPCKeyLoaded = 0;
uint8_t s_uOpenIPCKey1[crypto_box_SECRETKEYBYTES];
uint8_t s_uOpenIPCKey2[crypto_box_PUBLICKEYBYTES];

int load_openipc_keys(const char* szFileName)
{
   s_iOpenIPCKeyLoaded = 0;

   if ( (NULL == szFileName) || (0 == szFileName[0]) )
      return 0;

   FILE *fd = fopen(szFileName, "rb");

   if ( NULL == fd )
      return -1;

   if ( 1 != fread(s_uOpenIPCKey1, crypto_box_SECRETKEYBYTES, 1, fd) )
   {
      fclose(fd);
      return -2;
   }
   if ( 1 != fread(s_uOpenIPCKey2, crypto_box_SECRETKEYBYTES, 1, fd) )
   {
      fclose(fd);
      return -3;
   }

   fclose(fd);
   s_iOpenIPCKeyLoaded = 1;
   return 1;
}

uint8_t* get_openipc_key1()
{
   if ( ! s_iOpenIPCKeyLoaded )
      return NULL;
   return s_uOpenIPCKey1;
}

uint8_t* get_openipc_key2()
{
   if ( ! s_iOpenIPCKeyLoaded )
      return NULL;
   return s_uOpenIPCKey2;
}

#endif