/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../common/string_utils.h"
#include <stdio.h>
#include "models_connect_frequencies.h"


#define MAX_MODELS_ITEMS 50
u32 s_uLoadedModelsId[MAX_MODELS_ITEMS];
u32 s_uLoadedModelsConnectFreq[MAX_MODELS_ITEMS];
int s_iLoadedModelsFrequencyCount = 0;
bool s_bLoadedModelsFrequencies = false;

void _load_models_connect_frequencies()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_MODELS_CONNECT_FREQUENCIES);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      s_iLoadedModelsFrequencyCount = 0;
      s_bLoadedModelsFrequencies = true;
      return;
   }

   s_iLoadedModelsFrequencyCount = 0;

   while ( true )
   {
      if ( 2 != fscanf(fd, "%u %u", &(s_uLoadedModelsId[s_iLoadedModelsFrequencyCount]), &(s_uLoadedModelsConnectFreq[s_iLoadedModelsFrequencyCount])) )
         break;
      s_iLoadedModelsFrequencyCount++;
      if ( s_iLoadedModelsFrequencyCount >= MAX_MODELS_ITEMS )
         break;
   }
   fclose(fd);
   s_bLoadedModelsFrequencies = true;
}


void _save_models_connect_frequencies()
{
   if ( ! s_bLoadedModelsFrequencies )
      _load_models_connect_frequencies();

   // Remove models that do not exist anymore
   for( int i=0; i<s_iLoadedModelsFrequencyCount; i++ )
   {
      Model* pModel = findModelWithId(s_uLoadedModelsId[i], 1);
      if ( NULL != pModel )
         continue;
      log_line("Removed model VID %u from the list of main connect frequencies as it does not exist anymore.", s_uLoadedModelsId[i]);

      for( int k=i; k<s_iLoadedModelsFrequencyCount-1; k++ )
      {
         s_uLoadedModelsId[k] = s_uLoadedModelsId[k+1];
         s_uLoadedModelsConnectFreq[k] = s_uLoadedModelsConnectFreq[k+1];
      }
      s_iLoadedModelsFrequencyCount--;
   }

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_MODELS_CONNECT_FREQUENCIES);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save file containing models connect frequencies.");
      return;
   }

   for( int i=0; i<s_iLoadedModelsFrequencyCount; i++ )
      fprintf(fd, "%u %u\n", s_uLoadedModelsId[i], s_uLoadedModelsConnectFreq[i]);
   fclose(fd);
}

void set_model_main_connect_frequency(u32 uModelId, u32 uFreq)
{
   //if ( ! s_bLoadedModelsFrequencies )
   // Always load it as it could have been modified by another process
   _load_models_connect_frequencies();

   int iIndex = -1;
   for( int i=0; i<s_iLoadedModelsFrequencyCount; i++ )
      if ( s_uLoadedModelsId[i] == uModelId )
      {
         iIndex = i;
         break;
      }

   if ( iIndex != -1 )
      s_uLoadedModelsConnectFreq[iIndex] = uFreq;
   else
   {
      if ( s_iLoadedModelsFrequencyCount >= MAX_MODELS_ITEMS )
      {
         // No more room, remove the oldest one

         for( int i=0; i<s_iLoadedModelsFrequencyCount-1; i++ )
         {
            s_uLoadedModelsId[i] = s_uLoadedModelsId[i+1];
            s_uLoadedModelsConnectFreq[i] = s_uLoadedModelsConnectFreq[i+1];
         }
         s_iLoadedModelsFrequencyCount--;
      }
      s_uLoadedModelsId[s_iLoadedModelsFrequencyCount] = uModelId;
      s_uLoadedModelsConnectFreq[s_iLoadedModelsFrequencyCount] = uFreq;
      s_iLoadedModelsFrequencyCount++;
   }

   log_line("Did set main connect frequency for VID %u to: %s", uModelId, str_format_frequency(uFreq));
   _save_models_connect_frequencies();
}

u32 get_model_main_connect_frequency(u32 uModelId)
{
   if ( ! s_bLoadedModelsFrequencies )
      _load_models_connect_frequencies();

   for( int i=0; i<s_iLoadedModelsFrequencyCount; i++ )
      if ( s_uLoadedModelsId[i] == uModelId )
         return s_uLoadedModelsConnectFreq[i];

   return 0;
}
