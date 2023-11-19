#include "base.h"
#include "hardware.h"
#include "models.h"

Model* s_pModelsSpectator[MAX_MODELS_SPECTATOR];
int s_iModelsSpectatorCount = 0;

Model* s_pModels[MAX_MODELS];
int s_iModelsCount = 0;

Model* s_pCurrentModel = NULL;

static bool s_bLoadedAllModels = false;

bool loadAllModels()
{
   s_bLoadedAllModels = true;
  
   bool bSucceeded = true;

   if ( NULL != s_pCurrentModel )
      delete s_pCurrentModel;
   s_pCurrentModel = new Model();
   if ( ! s_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_softerror_and_alarm("Failed to load current model.");
      if( access( FILE_CURRENT_VEHICLE_MODEL, R_OK ) == -1 )
      if( access( FILE_CURRENT_VEHICLE_MODEL_BACKUP, R_OK ) == -1 )
         log_softerror_and_alarm("No current vehicle model file present.");
      s_pCurrentModel->resetToDefaults(true);
      bSucceeded = false;
   }

   log_line("Current model: [%s], VID: %u", s_pCurrentModel->getLongName(), s_pCurrentModel->vehicle_id);

   if ( hardware_is_vehicle() )
      return bSucceeded;

   s_iModelsCount = 0;
   s_iModelsSpectatorCount = 0;

   int count = load_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, 0);
   if (count < 0 )
      count = 0;
   log_line("Loading %d controller models...", count);
   for( int i=0; i<count; i++ )
   {
      char szFile[256];
      sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
      s_pModels[i] = new Model();
      if ( ! s_pModels[i]->loadFromFile(szFile, true) )
         break;

      if ( s_pCurrentModel->vehicle_id == s_pModels[i]->vehicle_id )
         s_pModels[i] = s_pCurrentModel;
      s_iModelsCount++;
   }
   log_line("Loaded %d controller models.", s_iModelsCount);

   s_iModelsSpectatorCount = 0;
   while( s_iModelsSpectatorCount < MAX_MODELS_SPECTATOR )
   {
      char szBuff[256];
      sprintf(szBuff, FILE_VEHICLE_SPECTATOR, s_iModelsSpectatorCount);
      s_pModelsSpectator[s_iModelsSpectatorCount] = new Model();
      if( access( szBuff, R_OK ) == -1 )
         break;
      if ( ! s_pModelsSpectator[s_iModelsSpectatorCount]->loadFromFile(szBuff, true) )
         break;
      if ( s_pCurrentModel->vehicle_id == s_pModelsSpectator[s_iModelsSpectatorCount]->vehicle_id )
         s_pModelsSpectator[s_iModelsSpectatorCount] = s_pCurrentModel;
      s_iModelsSpectatorCount++;
   }
   log_line("Loaded %d spectator models.", s_iModelsSpectatorCount);

   log_line("Loaded controller models:");
   for( int i=0; i<s_iModelsCount; i++ )
      log_line("Controller model %d: [%s], VID: %u", i+1, s_pModels[i]->getLongName(), s_pModels[i]->vehicle_id);

   return true;
}


bool saveCurrentModel()
{
   if ( NULL == s_pCurrentModel )
   {
      log_softerror_and_alarm("Current model is NULL. Can't save it.");
      return false;
   }

   log_line("Saving model id %u as current model", s_pCurrentModel->vehicle_id);
   s_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, hardware_is_station());

   if ( hardware_is_vehicle() )
      return true;

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( s_pModelsSpectator[i]->vehicle_id != s_pCurrentModel->vehicle_id )
         continue;
      char szBuff[256];
      sprintf(szBuff, FILE_VEHICLE_SPECTATOR, i);
      s_pModelsSpectator[i]->saveToFile(szBuff, hardware_is_station());
   }

   log_line("Saving %d controller models.", s_iModelsCount);
   save_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, s_iModelsCount);
   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( s_pModels[i]->vehicle_id != s_pCurrentModel->vehicle_id )
         continue;
      char szFile[256];
      sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
      s_pModels[i]->saveToFile(szFile, hardware_is_station());
   }
   return true;
}

Model* getCurrentModel()
{
   if ( NULL == s_pCurrentModel )
      s_pCurrentModel = new Model();
   return s_pCurrentModel;
}

bool reloadCurrentModel()
{
   if ( NULL == s_pCurrentModel )
      return false;

   if ( ! s_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
      return false;

   return true;
}

int getControllerModelsCount()
{
   return s_iModelsCount;
}

int getControllerModelsSpectatorCount()
{
   return s_iModelsSpectatorCount;
}


void deleteAllModels()
{
   s_iModelsSpectatorCount = 0;
   s_iModelsCount = 0;
   save_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, s_iModelsCount);
}


Model* getSpectatorModel(int iIndex)
{
   if ( iIndex < 0 || iIndex > s_iModelsSpectatorCount )
      return NULL;
   return s_pModelsSpectator[iIndex];
}


Model* addSpectatorModel(u32 vehicleId)
{
   int index = 0;
   for( index = 0; index < s_iModelsSpectatorCount; index++ )
      if ( s_pModelsSpectator[index]->vehicle_id == vehicleId )
      {
         // Move it to top of the list;
         Model* tmp = s_pModelsSpectator[index];
         for( int i=index-1; i >=0; i-- )
            if (i >=0 )
               s_pModelsSpectator[i+1] = s_pModelsSpectator[i];
         s_pModelsSpectator[0] = tmp;
         return s_pModelsSpectator[0];
      }

   // New vehicle, add it on top of the list, move the other ones down the list.

   for( int i=s_iModelsSpectatorCount-1; i >= 0; i-- )
   {
      if ( i < MAX_MODELS_SPECTATOR-1 )
         s_pModelsSpectator[i+1] = s_pModelsSpectator[i];
   }
   s_pModelsSpectator[0] = new Model();
   s_pModelsSpectator[0]->resetToDefaults(false);
   s_pModelsSpectator[0]->vehicle_id = vehicleId;

   s_iModelsSpectatorCount++;
   if ( s_iModelsSpectatorCount > MAX_MODELS_SPECTATOR )
      s_iModelsSpectatorCount = MAX_MODELS_SPECTATOR;

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      char szBuff[256];
      sprintf(szBuff, FILE_VEHICLE_SPECTATOR, i);
      s_pModelsSpectator[i]->saveToFile(szBuff, true);
   }

   return s_pModelsSpectator[0];
}

void moveSpectatorModelToTop(int index)
{
   if ( index < 0 || index >= s_iModelsSpectatorCount )
      return;
        
   Model* tmp = s_pModelsSpectator[index];
   for( int i=index-1; i >=0; i-- )
      if (i >=0 )
         s_pModelsSpectator[i+1] = s_pModelsSpectator[i];
   s_pModelsSpectator[0] = tmp;
}

Model* getModelAtIndex(int index) 
{
   if ( index < 0 || index >= MAX_MODELS )
      return NULL;
   return s_pModels[index];
}

Model* addNewModel()
{
   if ( s_iModelsCount >= MAX_MODELS-1 )
      return NULL;
   log_line("Adding a new model in the controller's models list...");
   s_pModels[s_iModelsCount] = new Model();
   s_pModels[s_iModelsCount]->resetToDefaults(true);
   
   char szBuff[256];
   sprintf(szBuff, FILE_VEHICLE_CONTROLLER, s_iModelsCount);
   s_pModels[s_iModelsCount]->saveToFile(szBuff, true);
   s_iModelsCount++;
   save_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, s_iModelsCount);
   
   log_line("Added a new model in the controller's models list, VID: %u", s_pModels[s_iModelsCount-1]->vehicle_id);
   return s_pModels[s_iModelsCount-1];
}

void replaceModel(int index, Model* pModel)
{
   if ( index < 0 || index >= MAX_MODELS-1 )
      return;

   if ( (NULL != s_pCurrentModel) && (NULL != s_pModels[index]) )
   if ( s_pCurrentModel->vehicle_id == s_pModels[index]->vehicle_id )
      s_pCurrentModel = pModel;
   
   if ( NULL == s_pModels[index] )
      log_line("Replacing controller model %d, which is NULL, with new VID %u, ptr: %X",
          index, pModel->vehicle_id, pModel);
   else
      log_line("Replacing controller model %d, VID %u, ptr: %X with new VID %u, ptr: %X",
          index, s_pModels[index]->vehicle_id, s_pModels[index],
          pModel->vehicle_id, pModel);

   s_pModels[index] = pModel;

   if ( NULL == s_pCurrentModel )
      log_line("Current model is NULL");
   else
      log_line("Current model VID: %u, ptr: %X", s_pCurrentModel->vehicle_id, s_pCurrentModel);
}

Model* findModelWithId(u32 vehicle_id)
{
   if ( ! s_bLoadedAllModels )
   {
      log_line("Models not loaded. Loading models first.");
      loadAllModels();
   }
   if ( NULL != s_pCurrentModel )
   if ( s_pCurrentModel->vehicle_id == vehicle_id )
      return s_pCurrentModel;

   for( int i=0; i<s_iModelsCount; i++ )
      if ( s_pModels[i]->vehicle_id == vehicle_id )
         return s_pModels[i];

   return NULL;
}

bool controllerHasModelWithId(u32 uVehicleId)
{
   for( int i=0; i<s_iModelsCount; i++ )
      if ( s_pModels[i]->vehicle_id == uVehicleId )
         return true;

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
      if ( s_pModelsSpectator[i]->vehicle_id == uVehicleId )
         return true;

   return false;
}

// Returns the current selected model, if one still remains

Model* deleteModel(Model* pModel)
{
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Tried to delete a NULL model object.");
      return s_pCurrentModel;
   }

   if ( hardware_is_vehicle() )
   {
      log_softerror_and_alarm("Tried to delete a model (VID %u, ptr: %X) on vehicle side.", pModel->vehicle_id, pModel);
      return s_pCurrentModel;
   }

   if ( 0 == s_iModelsCount )
   {
      log_softerror_and_alarm("Tried to delete a model when there are none in the list.");
      return s_pCurrentModel;
   }
   int pos = 0;
   for( pos=0; pos<s_iModelsCount; pos++ )
   {
      if ( s_pModels[pos]->vehicle_id != pModel->vehicle_id )
         continue;

      log_line("Deleting controller model index %d: VID %u, ptr: %X", pos+1, pModel->vehicle_id, pModel);
      
      if ( (NULL != s_pCurrentModel) && (s_pModels[pos]->vehicle_id == s_pCurrentModel->vehicle_id) )
      {
         log_line("Model to delete is also the current model. Delete it too.");
         unlink(FILE_CURRENT_VEHICLE_MODEL);
         unlink(FILE_CURRENT_VEHICLE_MODEL_BACKUP);
         log_line("Deleted current vehicle (VID %u, ptr: %X) model file: %s", s_pCurrentModel->vehicle_id, s_pCurrentModel, FILE_CURRENT_VEHICLE_MODEL);
         s_pCurrentModel = NULL;
      }
      
      for( int i=pos; i<s_iModelsCount-1; i++ )
         s_pModels[i] = s_pModels[i+1];
      s_iModelsCount--;

      log_line("Saving %d controller models.", s_iModelsCount);
      save_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, s_iModelsCount);
      for( int i=pos; i<s_iModelsCount; i++ )
      {
         char szFile[256];
         sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
         s_pModels[i]->saveToFile(szFile, false);
      }
      return s_pCurrentModel;
   }
   log_softerror_and_alarm("Tried to delete a model that is not in the list.");
   return s_pCurrentModel;
}

void saveControllerModel(Model* pModel)
{
   if ( NULL == pModel )
      return;

   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( s_pModels[i]->vehicle_id == pModel->vehicle_id )
      if ( s_pModels[i]->is_spectator == pModel->is_spectator )
      {
         log_line("Found matching vehicle in controller's list while saving a model. Save it in controller's models list too.");
         char szFile[256];
         sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
         pModel->saveToFile(szFile, true);
         s_pModels[i]->loadFromFile(szFile);
         break;
      }
   }

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( s_pModelsSpectator[i]->vehicle_id == pModel->vehicle_id )
      if ( s_pModelsSpectator[i]->is_spectator == pModel->is_spectator )
      {
         log_line("Found matching spectator vehicle in list.");
         char szFile[256];
         sprintf(szFile, FILE_VEHICLE_SPECTATOR, i);
         pModel->saveToFile(szFile, true);
         s_pModelsSpectator[i]->loadFromFile(szFile);
         break;
      }
   }

   if ( NULL != s_pCurrentModel )
   if ( pModel->vehicle_id == s_pCurrentModel->vehicle_id )
   {
      log_line("Saving model VID %u, ptr: %X, as current model", s_pCurrentModel->vehicle_id, s_pCurrentModel);
      pModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
      s_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL);
   }
}

Model* setControllerCurrentModel(u32 uVehicleId)
{
   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( s_pModels[i]->vehicle_id == uVehicleId )
      {
         s_pCurrentModel = s_pModels[i];
         log_line("Set VID %u, index %d as current controller model", uVehicleId, i);
         return s_pCurrentModel;
      }
   }

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( s_pModelsSpectator[i]->vehicle_id == uVehicleId )
      {
         s_pCurrentModel = s_pModelsSpectator[i];
         log_line("Set VID %u, index %d as current spectator model", uVehicleId, i);
         return s_pCurrentModel;
      }
   }
   return NULL;
}

void logControllerModels()
{
   log_line("Controller models: %d controller mode models, %d spectator mode models, has current model: %s",
      s_iModelsCount, s_iModelsSpectatorCount, s_pCurrentModel != NULL? "yes": "no");

   for( int i=0; i<s_iModelsCount; i++ )
   {
      if ( NULL == s_pModels[i] )
         log_error_and_alarm("Controller model %d: is NULL.", i+1);
      else
         log_line("Controller model %d: VID %u, ptr: %X, is spectator: %s, must sync: %s",
            i+1, s_pModels[i]->vehicle_id, s_pModels[i], s_pModels[i]->is_spectator?"yes":"no", s_pModels[i]->b_mustSyncFromVehicle?"yes":"no");
   }

   for( int i=0; i<s_iModelsSpectatorCount; i++ )
   {
      if ( NULL == s_pModels[i] )
         log_error_and_alarm("Spectator model %d: is NULL.", i+1);
      else
         log_line("Spectator model %d: VID %u, ptr: %X, is spectator: %s, must sync: %s",
         i+1, s_pModelsSpectator[i]->vehicle_id, s_pModelsSpectator[i], s_pModelsSpectator[i]->is_spectator?"yes":"no", s_pModelsSpectator[i]->b_mustSyncFromVehicle?"yes":"no");
   }
   
   if ( NULL != s_pCurrentModel )
      log_line("Current model: VID %u, ptr: %X, is spectator: %s, must sync: %s",
         s_pCurrentModel->vehicle_id, s_pCurrentModel, s_pCurrentModel->is_spectator?"yes":"no", s_pCurrentModel->b_mustSyncFromVehicle?"yes":"no");
}