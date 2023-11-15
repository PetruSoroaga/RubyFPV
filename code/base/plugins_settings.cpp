/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "base.h"
#include "config.h"
#include "plugins_settings.h"
#include "hardware.h"
#include "hw_procs.h"

PluginsSettings s_PluginsSettings;
int s_PluginsSettingsLoaded = 0;

void reset_PluginsSettings()
{
   memset(&s_PluginsSettings, 0, sizeof(s_PluginsSettings));
   s_PluginsSettings.nPluginsCount = 0;
   log_line("Reseted Plugins settings.");
}

int save_PluginsSettings()
{
   FILE* fd = fopen(FILE_OSD_PLUGINS_SETTINGS, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save plugins settings to file: %s",FILE_OSD_PLUGINS_SETTINGS);
      return 0;
   }

   fprintf(fd, "%s\n", PLUGINS_SETTINGS_STAMP_ID);
   fprintf(fd, "%d\n", s_PluginsSettings.nPluginsCount);

   for( int iPlugin=0; iPlugin<s_PluginsSettings.nPluginsCount; iPlugin++ )
   {
      fprintf(fd, "%s\n", s_PluginsSettings.pPlugins[iPlugin]->szName);
      fprintf(fd, "%s\n", s_PluginsSettings.pPlugins[iPlugin]->szUID);
      fprintf(fd, "%d %d %d\n", s_PluginsSettings.pPlugins[iPlugin]->nEnabled, s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount, s_PluginsSettings.pPlugins[iPlugin]->nModels);

      for( int iModel=0; iModel<s_PluginsSettings.pPlugins[iPlugin]->nModels; iModel++ )
      {
         fprintf(fd, "%u\n", s_PluginsSettings.pPlugins[iPlugin]->uModelsIds[iModel]);

         for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
            fprintf(fd, "%f %f %f %f\n", s_PluginsSettings.pPlugins[iPlugin]->fXPos[iModel][k], s_PluginsSettings.pPlugins[iPlugin]->fYPos[iModel][k], s_PluginsSettings.pPlugins[iPlugin]->fWidth[iModel][k], s_PluginsSettings.pPlugins[iPlugin]->fHeight[iModel][k] );

         for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
         {
            for( int j=0; j<s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount; j++ )
               fprintf(fd, "%d ", s_PluginsSettings.pPlugins[iPlugin]->nSettings[iModel][k][j]);
            fprintf(fd, "\n");
         }
      }
   }
   fclose(fd);

   log_line("Saved plugins settings (%d plugins) to file: %s", s_PluginsSettings.nPluginsCount, FILE_OSD_PLUGINS_SETTINGS);
   return 1;
}

int load_PluginsSettings()
{
   log_line("Loading plugin settings from file: %s", FILE_OSD_PLUGINS_SETTINGS);
   s_PluginsSettingsLoaded = 1;
   FILE* fd = fopen(FILE_OSD_PLUGINS_SETTINGS, "r");
   if ( NULL == fd )
   {
      reset_PluginsSettings();
      log_softerror_and_alarm("Failed to load plugins settings from file: %s (missing file)",FILE_OSD_PLUGINS_SETTINGS);
      return 0;
   }

   int failed = 0;
   int tmp1 = 0;
   int tmp2 = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, PLUGINS_SETTINGS_STAMP_ID) )
      failed = 1;

   if ( failed )
   {
      reset_PluginsSettings();
      log_softerror_and_alarm("Failed to load plugins settings from file: %s (invalid config file version)",FILE_OSD_PLUGINS_SETTINGS);
      fclose(fd);
      return 0;
   }

   if ( (!failed) && (1 != fscanf(fd, "%d", &s_PluginsSettings.nPluginsCount)) )
      { s_PluginsSettings.nPluginsCount = 0; failed = 1; log_softerror_and_alarm("Load plugins settings: Invalid plugins count"); }

   if ( s_PluginsSettings.nPluginsCount < 0 || s_PluginsSettings.nPluginsCount >= MAX_OSD_PLUGINS )
      { s_PluginsSettings.nPluginsCount = 0; failed = 1; log_softerror_and_alarm("Load plugins settings: Invalid plugins count2"); }

   if ( ! failed )
   for( int iPlugin=0; iPlugin<s_PluginsSettings.nPluginsCount; iPlugin++ )
   {
      s_PluginsSettings.pPlugins[iPlugin] = (SinglePluginSettings*)malloc(sizeof(SinglePluginSettings));
      s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount = 0;
      s_PluginsSettings.pPlugins[iPlugin]->nModels = 0;
      s_PluginsSettings.pPlugins[iPlugin]->szName[0] = 0;
      s_PluginsSettings.pPlugins[iPlugin]->szUID[0] = 0;

      char * line = NULL;
      size_t len = 0;
      ssize_t readline;

      readline = getline(&line, &len, fd);
      line = NULL;
      len = 0;
      readline = getline(&line, &len, fd);
      if ( readline < 0 )
         { s_PluginsSettings.pPlugins[iPlugin]->szName[0] = 0; failed = 1; }
      else if ( readline > 0 )
          strlcpy(s_PluginsSettings.pPlugins[iPlugin]->szName, line, MAX_PLUGIN_NAME_LENGTH);

      if ( 10 == s_PluginsSettings.pPlugins[iPlugin]->szName[strlen(s_PluginsSettings.pPlugins[iPlugin]->szName)-1] )
         s_PluginsSettings.pPlugins[iPlugin]->szName[strlen(s_PluginsSettings.pPlugins[iPlugin]->szName)-1] = 0;
      if ( 13 == s_PluginsSettings.pPlugins[iPlugin]->szName[strlen(s_PluginsSettings.pPlugins[iPlugin]->szName)-1] )
         s_PluginsSettings.pPlugins[iPlugin]->szName[strlen(s_PluginsSettings.pPlugins[iPlugin]->szName)-1] = 0;
      if ( 10 == s_PluginsSettings.pPlugins[iPlugin]->szName[strlen(s_PluginsSettings.pPlugins[iPlugin]->szName)-1] )
         s_PluginsSettings.pPlugins[iPlugin]->szName[strlen(s_PluginsSettings.pPlugins[iPlugin]->szName)-1] = 0;

      line = NULL;
      len = 0;
      readline = getline(&line, &len, fd);
      if ( readline < 0 )
         { s_PluginsSettings.pPlugins[iPlugin]->szUID[0] = 0; failed = 1; }
      else if ( readline > 0 )
          strlcpy(s_PluginsSettings.pPlugins[iPlugin]->szUID, line, MAX_PLUGIN_NAME_LENGTH);

      if ( 10 == s_PluginsSettings.pPlugins[iPlugin]->szUID[strlen(s_PluginsSettings.pPlugins[iPlugin]->szUID)-1] )
         s_PluginsSettings.pPlugins[iPlugin]->szUID[strlen(s_PluginsSettings.pPlugins[iPlugin]->szUID)-1] = 0;
      if ( 13 == s_PluginsSettings.pPlugins[iPlugin]->szUID[strlen(s_PluginsSettings.pPlugins[iPlugin]->szUID)-1] )
         s_PluginsSettings.pPlugins[iPlugin]->szUID[strlen(s_PluginsSettings.pPlugins[iPlugin]->szUID)-1] = 0;
      if ( 10 == s_PluginsSettings.pPlugins[iPlugin]->szUID[strlen(s_PluginsSettings.pPlugins[iPlugin]->szUID)-1] )
         s_PluginsSettings.pPlugins[iPlugin]->szUID[strlen(s_PluginsSettings.pPlugins[iPlugin]->szUID)-1] = 0;

      if ( failed )
         log_softerror_and_alarm("Load plugins settings: Invalid plugin name or GUID");
      if ( (!failed) && (3 != fscanf(fd, "%d %d %d", &s_PluginsSettings.pPlugins[iPlugin]->nEnabled, &s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount, &s_PluginsSettings.pPlugins[iPlugin]->nModels) ) )
         { failed = 1; log_softerror_and_alarm("Load plugins settings: Invalid plugin settings [%d].", iPlugin); };

      if ( s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount < 0 )
         s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount = 0;
      if ( s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount > MAX_PLUGIN_SETTINGS )
         s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount = MAX_PLUGIN_SETTINGS;

      if ( s_PluginsSettings.pPlugins[iPlugin]->nModels < 0 )
         s_PluginsSettings.pPlugins[iPlugin]->nModels = 0;
      if ( s_PluginsSettings.pPlugins[iPlugin]->nModels > MAX_MODELS )
         s_PluginsSettings.pPlugins[iPlugin]->nModels = MAX_MODELS;

      for( int iModel=0; iModel<s_PluginsSettings.pPlugins[iPlugin]->nModels; iModel++ )
      {
         if ( (!failed) && (1 != fscanf(fd, "%u", &s_PluginsSettings.pPlugins[iPlugin]->uModelsIds[iModel]) ) )
            { failed = 1; log_softerror_and_alarm("Load plugins settings: Invalid plugin model id [%d/%d].", iPlugin,iModel); }

         for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
            if ( (!failed) && (4 != fscanf(fd, "%f %f %f %f", &s_PluginsSettings.pPlugins[iPlugin]->fXPos[iModel][k], &s_PluginsSettings.pPlugins[iPlugin]->fYPos[iModel][k], &s_PluginsSettings.pPlugins[iPlugin]->fWidth[iModel][k], &s_PluginsSettings.pPlugins[iPlugin]->fHeight[iModel][k]) ) )
               { failed = 1; log_softerror_and_alarm("Load plugins settings: Invalid plugin OSD layout [%d/%d/%d].", iPlugin, iModel, k); }

         if ( !failed )
         for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
         for( int j=0; j<s_PluginsSettings.pPlugins[iPlugin]->nSettingsCount; j++ )
         {
            if ( (!failed) && (1 != fscanf(fd, "%d", &(s_PluginsSettings.pPlugins[iPlugin]->nSettings[iModel][k][j])) ) )
               { failed = 1; log_softerror_and_alarm("Load plugins settings: Invalid plugin setting [%d/%d/%d/%d].", iPlugin, iModel, k, j); };
         }
      }

      if ( failed )
      {
         s_PluginsSettings.nPluginsCount = iPlugin;
         break;
      }
   }

   fclose(fd);

   if ( failed )
      log_softerror_and_alarm("Incomplete/Invalid plugins settings file %s", FILE_OSD_PLUGINS_SETTINGS);
   else
      log_line("Loaded plugins settings from file: %s", FILE_OSD_PLUGINS_SETTINGS);
   return 1;
}

PluginsSettings* get_PluginsSettings()
{
   if ( ! s_PluginsSettingsLoaded )
      load_PluginsSettings();
   return &s_PluginsSettings;
}

SinglePluginSettings* pluginsGetByUID(const char* szUID)
{
   if ( NULL == szUID )
      return NULL;

   for( int i=0; i<s_PluginsSettings.nPluginsCount; i++ )
      if ( NULL != s_PluginsSettings.pPlugins[i] )
      if ( 0 == strcmp(s_PluginsSettings.pPlugins[i]->szUID, szUID) )
         return s_PluginsSettings.pPlugins[i];

   return NULL;
}

SinglePluginSettings* pluginsSettingsAddNew(const char* szName, const char* szUID)
{
   if ( s_PluginsSettings.nPluginsCount >= MAX_OSD_PLUGINS )
      return NULL;

   s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount] = (SinglePluginSettings*)malloc(sizeof(SinglePluginSettings));
   if ( NULL == s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount] )
      return NULL;

    strlcpy(s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->szName, szName, MAX_PLUGIN_NAME_LENGTH);
    strlcpy(s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->szUID, szUID, MAX_PLUGIN_NAME_LENGTH);
   s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->nEnabled = 1;
   s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->nSettingsCount = 0;
   s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->nModels = 0;

   for( int iModel=0; iModel<MAX_MODELS; iModel++ )
      s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->uModelsIds[iModel] = 0;

   for( int iModel=0; iModel<MAX_MODELS; iModel++ )
   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   for( int k=0; k<MAX_PLUGIN_SETTINGS; k++ )
      s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount]->nSettings[iModel][i][k] = 0;

   s_PluginsSettings.nPluginsCount++;
   return s_PluginsSettings.pPlugins[s_PluginsSettings.nPluginsCount-1];
}

void delete_PluginByUID(const char* szUID)
{
   if ( NULL == szUID )
      return;

   for( int i=0; i<s_PluginsSettings.nPluginsCount; i++ )
   {
      if ( NULL != s_PluginsSettings.pPlugins[i] )
      if ( 0 == strcmp(s_PluginsSettings.pPlugins[i]->szUID, szUID) )
      {
         for( int k=i; k<s_PluginsSettings.nPluginsCount-1; k++ )
         {
            s_PluginsSettings.pPlugins[k] = s_PluginsSettings.pPlugins[k+1];
         }
         s_PluginsSettings.nPluginsCount--;
         return;
      }
   }
}

int doesPluginHasModelSettings(SinglePluginSettings* pPlugin, Model* pModel)
{
   if ( NULL == pModel || NULL == pPlugin )
      return 0;

   for( int i=0; i<pPlugin->nModels; i++ )
      if ( pPlugin->uModelsIds[i] == pModel->vehicle_id )
         return 1;

   return 0;
}

int getPluginModelSettingsIndex(SinglePluginSettings* pPlugin, Model* pModel)
{
   if ( NULL == pModel || NULL == pPlugin )
      return 0;

   for( int i=0; i<pPlugin->nModels; i++ )
      if ( pPlugin->uModelsIds[i] == pModel->vehicle_id )
         return i;

   if ( pPlugin->nModels >= MAX_MODELS )
   {
      // Discard oldest model
      for( int i=0; i<MAX_MODELS-1; i++ )
      {
         pPlugin->uModelsIds[i] = pPlugin->uModelsIds[i+1];
         for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
         {
            pPlugin->fXPos[i][k] = pPlugin->fXPos[i+1][k];
            pPlugin->fYPos[i][k] = pPlugin->fYPos[i+1][k];
            pPlugin->fWidth[i][k] = pPlugin->fWidth[i+1][k];
            pPlugin->fHeight[i][k] = pPlugin->fHeight[i+1][k];
            for( int j=0; j<MAX_PLUGIN_SETTINGS; j++ )
               pPlugin->nSettings[i][k][j] = pPlugin->nSettings[i+1][k][j];
         }
      }
      pPlugin->nModels--;   
   }

   // Add the new vehicle in the list

   pPlugin->uModelsIds[pPlugin->nModels] = pModel->vehicle_id;

   if ( pPlugin->nModels > 0 )
   {
      // Copy setting from last used model

      for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
      {
         pPlugin->fXPos[pPlugin->nModels][k] = pPlugin->fXPos[pPlugin->nModels-1][k];
         pPlugin->fYPos[pPlugin->nModels][k] = pPlugin->fYPos[pPlugin->nModels-1][k];
         pPlugin->fWidth[pPlugin->nModels][k] = pPlugin->fWidth[pPlugin->nModels-1][k];
         pPlugin->fHeight[pPlugin->nModels][k] = pPlugin->fHeight[pPlugin->nModels-1][k];
         for( int j=0; j<MAX_PLUGIN_SETTINGS; j++ )
            pPlugin->nSettings[pPlugin->nModels][k][j] = pPlugin->nSettings[pPlugin->nModels-1][k][j];
      }
   }

   pPlugin->nModels++;
   return pPlugin->nModels-1;
}

void deletePluginModelSettings(u32 uModelId)
{
   if ( 0 == uModelId || MAX_U32 == uModelId )
      return;

   for( int i=0; i<s_PluginsSettings.nPluginsCount; i++ )
   {
      for( int iModel=0; iModel<s_PluginsSettings.pPlugins[i]->nModels; iModel++ )
      {
         if ( (NULL != s_PluginsSettings.pPlugins[i]) && (s_PluginsSettings.pPlugins[i]->uModelsIds[iModel] == uModelId) )
         {
            for( int k=iModel; k<s_PluginsSettings.pPlugins[i]->nModels-1; k++ )
            {
               for( int j=0; j<MODEL_MAX_OSD_PROFILES; j++ )
               {
               s_PluginsSettings.pPlugins[i]->fXPos[k][j] = s_PluginsSettings.pPlugins[i]->fXPos[k+1][j];
               s_PluginsSettings.pPlugins[i]->fYPos[k][j] = s_PluginsSettings.pPlugins[i]->fYPos[k+1][j];
               s_PluginsSettings.pPlugins[i]->fWidth[k][j] = s_PluginsSettings.pPlugins[i]->fWidth[k+1][j];
               s_PluginsSettings.pPlugins[i]->fHeight[k][j] = s_PluginsSettings.pPlugins[i]->fHeight[k+1][j];
               for( int p=0; p<MAX_PLUGIN_SETTINGS; p++ )
                  s_PluginsSettings.pPlugins[i]->nSettings[k][j][p] = s_PluginsSettings.pPlugins[i]->nSettings[k+1][j][p];
               }
            }
            s_PluginsSettings.pPlugins[i]->nModels--;
            break;
         }
      }
   }
}
