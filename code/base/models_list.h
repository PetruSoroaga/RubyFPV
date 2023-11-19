#pragma once
#include "base.h"
#include "config.h"
#include "models.h"


bool loadAllModels();
bool saveCurrentModel();
Model* getCurrentModel();
bool reloadCurrentModel();

int getControllerModelsCount();
int getControllerModelsSpectatorCount();

void deleteAllModels();

Model* getSpectatorModel(int iIndex);
Model* addSpectatorModel(u32 vehicleId);
void moveSpectatorModelToTop(int index);

Model* getModelAtIndex(int index);
Model* addNewModel();
void replaceModel(int index, Model* pModel);
Model* findModelWithId(u32 vehicle_id);
bool controllerHasModelWithId(u32 uVehicleId);
Model* deleteModel(Model* pModel);
void saveControllerModel(Model* pModel);
Model* setControllerCurrentModel(u32 uVehicleId);

void logControllerModels();

