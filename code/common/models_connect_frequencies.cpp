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
   FILE* fd = fopen(FILE_CONFIG_MODELS_CONNECT_FREQUENCIES, "r");
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
      Model* pModel = findModelWithId( s_uLoadedModelsId[i] );
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

   FILE* fd = fopen(FILE_CONFIG_MODELS_CONNECT_FREQUENCIES, "w");
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
