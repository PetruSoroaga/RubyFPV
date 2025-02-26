/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "base.h"
#include "config.h"
#include "encr.h"
#include "../radio/radiopackets2.h"

#define ENC_BLOCK_SIZE 8
#define ENC_KEY_INIT_SEED 23

u8 s_epp[MAX_PASS_LENGTH+1];
u8 s_eppl = 0;

int lpp(char* szOutputBuffer, int maxLength)
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_ENCRYPTION_PASS);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return 0;

   u8 sBlockSeed[ENC_BLOCK_SIZE];
   u8 sBlockKey[ENC_BLOCK_SIZE];
   u8 sBlockEnc[2*ENC_BLOCK_SIZE];
   u8 sBlockDec[ENC_BLOCK_SIZE];

   for( int i=0; i<ENC_BLOCK_SIZE; i++ )
   {
      sBlockKey[i] = i+11;
      sBlockSeed[i] = ENC_KEY_INIT_SEED;
   }

   char szBuffer[1024];
   int pos = 0;
   szBuffer[pos] = 0;

   while ( 2*ENC_BLOCK_SIZE == fread(sBlockEnc, 1, 2*ENC_BLOCK_SIZE, fd) )
   {
      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
      {
          u8 val = sBlockEnc[k*2] - 180;
          u8 val2 = sBlockEnc[k*2+1] - 180;
          sBlockEnc[k] = val | (val2<<4);
      }

      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
      {
         sBlockDec[k] = (sBlockEnc[k] ^ sBlockKey[k]) ^ sBlockSeed[k];
         szBuffer[pos] = sBlockDec[k];
         pos++;
      }
      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
         sBlockSeed[k] = sBlockEnc[k];
   }

   if ( pos == 0 )
      return 0;

   szBuffer[pos] = 0;
   s_eppl = pos;
   strncpy((char*)s_epp, szBuffer, MAX_PASS_LENGTH);
   s_epp[MAX_PASS_LENGTH] = 0;

   if ( NULL != szOutputBuffer )
      strncpy(szOutputBuffer, szBuffer, maxLength);

   return 1;
}

int spp(char* szBuffer)
{
   if ( NULL == szBuffer || 0 == szBuffer[0] )
      return 0;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_ENCRYPTION_PASS);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
      return 0;

   s_eppl = strlen(szBuffer);
   strncpy((char*)s_epp, szBuffer, MAX_PASS_LENGTH);
   s_epp[MAX_PASS_LENGTH] = 0;

   u8 sBlockSeed[ENC_BLOCK_SIZE];
   u8 sBlockInput[ENC_BLOCK_SIZE];
   u8 sBlockKey[ENC_BLOCK_SIZE];
   u8 sBlockEnc[ENC_BLOCK_SIZE];

   for( int i=0; i<ENC_BLOCK_SIZE; i++ )
   {
      sBlockKey[i] = i+11;
      sBlockSeed[i] = ENC_KEY_INIT_SEED;
   }

   int len = strlen(szBuffer);
   int pos = 0;
   while (pos < len)
   {
      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
      {
         if ( pos+k < len )
            sBlockInput[k] = szBuffer[pos+k];
         else
            sBlockInput[k] = 0;
      }
      pos += ENC_BLOCK_SIZE;

      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
         sBlockEnc[k] = (sBlockInput[k] ^ sBlockSeed[k]) ^ sBlockKey[k];

      //fwrite(sBlockEnc, 1, ENC_BLOCK_SIZE, fd);
      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
      {
         u8 val = sBlockEnc[k] & 0xF;
         val += 180;
         fwrite(&val, 1,1, fd);

         val = (sBlockEnc[k] >> 4 ) & 0xF;
         val += 180;
         fwrite(&val, 1,1, fd);
      }

      for( int k=0; k<ENC_BLOCK_SIZE; k++ )
         sBlockSeed[k] = sBlockEnc[k];
   }

   fclose(fd);
   return 1;
}

void rpp()
{
   s_eppl = 0;
   s_epp[0] = 0;
}

u8* gpp(int* pLen)
{
   if ( NULL != pLen )
      *pLen = (int)s_eppl;
   return s_epp;
}

int hpp()
{
   if ( 0 < s_eppl )
      return 1;
   return 0;
}

int epp(u8* pData, int len)
{
   if ( NULL == pData || len <= 0 )
      return 0;
   if ( 0 == s_eppl )
      return 1;

   for(int pos=0; pos < len; pos++ )
   {
      //*pData = (*pData)+1;
      *pData = (*pData) ^ s_epp[pos%s_eppl];
      pData++;
   }
   return 1;
}

int dpp(u8* pData, int len)
{
   if ( NULL == pData || len <= 0 )
      return 0;
   if ( 0 == s_eppl )
      return 1;

   for(int pos=0; pos < len; pos++ )
   {
      //*pData = (*pData)-1;
      *pData = (*pData) ^ s_epp[pos%s_eppl];
      pData++;
   }
   return 1;
}
