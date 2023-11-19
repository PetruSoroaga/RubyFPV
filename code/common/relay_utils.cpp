#include "../base/base.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "relay_utils.h"

bool relay_controller_is_vehicle_id_relayed_vehicle(Model* pMainModel, u32 uVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uVehicleId) )
      return false;

   if ( pMainModel->vehicle_id == uVehicleId )
      return false;
   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || ( pMainModel->relay_params.uRelayedVehicleId != uVehicleId) )
      return false;

   return true;
}

bool relay_controller_must_display_video_from(Model* pMainModel, u32 uVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uVehicleId) )
      return false;

   bool bHasValidRelayedVehicle = false;

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) && (pMainModel->relay_params.uRelayedVehicleId != 0) )
   if ( pMainModel->relay_params.uRelayedVehicleId != pMainModel->vehicle_id )
      bHasValidRelayedVehicle = true;

   if ( ! bHasValidRelayedVehicle )
      return true;

   if ( uVehicleId != pMainModel->vehicle_id )
   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) )
      return false;

   if ( (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) ||
        (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) )
      return true;

   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE )
   if ( uVehicleId == pMainModel->relay_params.uRelayedVehicleId )
      return true;

   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_MAIN )
   if ( uVehicleId == pMainModel->vehicle_id )
      return true;

   return false;
}

bool relay_controller_must_display_remote_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return NULL;
   return relay_controller_must_display_video_from(pMainModel, pMainModel->relay_params.uRelayedVehicleId);
}

bool relay_controller_must_display_main_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return NULL;
   return relay_controller_must_display_video_from(pMainModel, pMainModel->vehicle_id);
}

Model* relay_controller_get_relayed_vehicle_model(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return NULL;

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || (0 == pMainModel->relay_params.uRelayedVehicleId) )
      return NULL;
   if ( pMainModel->relay_params.uRelayedVehicleId == pMainModel->vehicle_id )
      return NULL;

   Model* pModel = findModelWithId(pMainModel->relay_params.uRelayedVehicleId);
   return pModel;
}


bool relay_controller_must_forward_telemetry_from(Model* pMainModel, u32 uVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uVehicleId) )
      return false;

   if ( pMainModel->vehicle_id == uVehicleId )
      return true;

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || (pMainModel->relay_params.uRelayedVehicleId == 0) )
      return false;

   if ( pMainModel->relay_params.uRelayedVehicleId != uVehicleId )
      return false;

   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY) )
      return false;

   return true;
}

bool relay_vehicle_must_forward_video_from_relayed_vehicle(Model* pMainModel, u32 uVehicleId)
{
   if ( (NULL == pMainModel) || (0 == uVehicleId) )
      return false;

   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) )
      return false;

   if ( pMainModel->relay_params.uRelayedVehicleId != uVehicleId )
      return false;
      
   if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) )
   if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) )
   if ( ! (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) )
      return false;

   return true;
}

bool relay_vehicle_must_forward_own_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return false;

   // Standalone and not a relayed node

   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0 ) || (pMainModel->relay_params.uRelayedVehicleId == 0) )
   if ( !( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAYED_NODE) )
      return true;

   // Standalone vehicle or relayed vehicle always sends it's own video as we can have spectators or controllers connected to it
   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0 ) || (pMainModel->relay_params.uRelayedVehicleId == 0) )
      return true;

   // Relay node

   bool bIsRelayNode = false;
   if ( pMainModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( pMainModel->relay_params.uRelayedVehicleId != 0 )
      bIsRelayNode = true;

   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAY_NODE )
      bIsRelayNode = true;

   if ( bIsRelayNode )
   {
      if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO ) )
         return true;

      if ( (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_MAIN) ||
           (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) ||
           (pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) )
         return true;
      return false;
   }

   // Relayed node
   if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_IS_RELAYED_NODE )
   {
      if ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_MAIN )
         return false;
      return true;
   }

   return true;
}

bool relay_vehicle_must_forward_relayed_video(Model* pMainModel)
{
   if ( NULL == pMainModel )
      return false;
   
   if ( (pMainModel->relay_params.isRelayEnabledOnRadioLinkId < 0 ) || (0 == pMainModel->relay_params.uRelayedVehicleId) )
      return false;

   if ( ! (pMainModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) )
      return false;

   if ( ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE ) ||
        ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN ) ||
        ( pMainModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE ) )
      return true;
   return false;
}