/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include <stdio.h>
#include "models_connect_frequencies.h"


#define MAX_MODELS_ITEMS 50
u32 s_uLoadedModelsId[MAX_MODELS_ITEMS];
u32 s_uLoadedModelsConnectFreq[MAX_MODELS_ITEMS];
int s_iLoadedModelsCount = 0;
bool s_bLoadedModels = false;

void _load_models_connect_frequencies()
{
   FILE* fd = fopen(FILE_CONFIG_MODELS_CONNECT_FREQUENCIES, "r");
   if ( NULL == fd )
   {
      s_iLoadedModelsCount = 0;
      s_bLoadedModels = true;
      return;
   }

   s_iLoadedModelsCount = 0;

   while ( true )
   {
      if ( 2 != fscanf(fd, "%u %u", &(s_uLoadedModelsId[s_iLoadedModelsCount]), &(s_uLoadedModelsConnectFreq[s_iLoadedModelsCount])) )
         break;
      s_iLoadedModelsCount++;
      if ( s_iLoadedModelsCount >= MAX_MODELS_ITEMS )
         break;
   }
   fclose(fd);
   s_bLoadedModels = true;
}


void _save_models_connect_frequencies()
{
   if ( ! s_bLoadedModels )
      _load_models_connect_frequencies();

   FILE* fd = fopen(FILE_CONFIG_MODELS_CONNECT_FREQUENCIES, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save file containing models connect frequencies.");
      return;
   }

   for( int i=0; i<s_iLoadedModelsCount; i++ )
      fprintf(fd, "%u %u\n", s_uLoadedModelsId[i], s_uLoadedModelsConnectFreq[i]);
   fclose(fd);
}

void set_model_main_connect_frequency(u32 uModelId, u32 uFreq)
{
   if ( ! s_bLoadedModels )
      _load_models_connect_frequencies();

   int iIndex = -1;
   for( int i=0; i<s_iLoadedModelsCount; i++ )
      if ( s_uLoadedModelsId[i] == uModelId )
      {
         iIndex = i;
         break;
      }

   if ( iIndex != -1 )
      s_uLoadedModelsConnectFreq[iIndex] = uFreq;
   else
   {
      if ( s_iLoadedModelsCount >= MAX_MODELS_ITEMS )
      {
         // No more room, remove the oldest one

         for( int i=0; i<s_iLoadedModelsCount-1; i++ )
         {
            s_uLoadedModelsId[i] = s_uLoadedModelsId[i+1];
            s_uLoadedModelsConnectFreq[i] = s_uLoadedModelsConnectFreq[i+1];
         }
         s_iLoadedModelsCount--;
      }
      s_uLoadedModelsId[s_iLoadedModelsCount] = uModelId;
      s_uLoadedModelsConnectFreq[s_iLoadedModelsCount] = uFreq;
      s_iLoadedModelsCount++;
   }
   _save_models_connect_frequencies();
}

u32 get_model_main_connect_frequency(u32 uModelId)
{
   if ( ! s_bLoadedModels )
      _load_models_connect_frequencies();

   for( int i=0; i<s_iLoadedModelsCount; i++ )
      if ( s_uLoadedModelsId[i] == uModelId )
         return s_uLoadedModelsConnectFreq[i];

   return 0;
}
