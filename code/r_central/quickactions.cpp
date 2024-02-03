/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
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

#include "../base/base.h"
#include "../base/flags.h"
#include "../base/models_list.h"
#include "../base/ctrl_preferences.h"
#include "../common/string_utils.h"
#include "../common/favorites.h"
#include "quickactions.h"
#include "colors.h"
#include "ruby_central.h"
#include "events.h"
#include "pairing.h"
#include "handle_commands.h"
#include "link_watch.h"
#include "warnings.h"
#include "popup.h"
#include "osd_common.h"
#include "fonts.h"
#include "media.h"
#include "shared_vars.h"
#include "timers.h"
#include "process_router_messages.h"
#include "../radio/radiopackets2.h"

u32 s_uTimeLastQuickActionPress = 0;
u32 s_uLastQuickActionSwitchVideoProfile = 0;

bool quickActionCheckVehicle(const char* szText)
{
   bool bHasVehicle = false;
   if ( pairing_isStarted() && (NULL != g_pCurrentModel) )
   if ( link_is_vehicle_online_now(g_pCurrentModel->vehicle_id) )
   if ( link_has_received_main_vehicle_ruby_telemetry() )
      bHasVehicle = true;

   if ( bHasVehicle )
      return true;

   char szBuff[256];
   if ( NULL == szText || 0 == szText[0] )
      strcpy(szBuff, "You must be connected to a vehicle to use this Quick Action button function");
   else
      sprintf(szBuff, "You must be connected to a vehicle to %s", szText);

   Popup* p = new Popup(szBuff, 0.1,0.7, 0.54, 4);
   p->setCentered();
   p->setIconId(g_idIconInfo, get_Color_IconWarning());
   popups_add_topmost(p);
   return false;  
}

void executeQuickActionTakePicture()
{
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
      return;
   Preferences* p = get_Preferences();
   if ( NULL == p )
      return;
   s_uTimeLastQuickActionPress = get_current_timestamp_ms();
   media_take_screenshot(p->iAddOSDOnScreenshots);
}

void executeQuickActionRecord()
{
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
      return;
   s_uTimeLastQuickActionPress = get_current_timestamp_ms();

   if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_OPENIPC )
   {
      warnings_add(g_pCurrentModel->vehicle_id, "Can't record video for OpenIPC vehicles.");
      return;
   }

   if ( g_bVideoRecordingStarted )
   {
      ruby_stop_recording();
   }
   else
   {
      if ( ! link_is_vehicle_online_now(g_pCurrentModel->vehicle_id) )
      {
         Popup* p = new Popup("You must be connected to a vehicle to start video recording.", 0.1,0.8, 0.54, 5);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
      }
      else
         ruby_start_recording();
   }
}

void executeQuickActionSwitchVideoProfile()
{
   if ( g_pCurrentModel->is_spectator )
   {
      warnings_add(0, "Can't execute video profile switch while in spectator mode.");
      return;
   }
   s_uLastQuickActionSwitchVideoProfile++;
   if ( s_uLastQuickActionSwitchVideoProfile > 2 )
      s_uLastQuickActionSwitchVideoProfile = 0;

   if ( 0 == s_uLastQuickActionSwitchVideoProfile )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_AUTO, 0, NULL, 0);
      warnings_add(0, "Dev: Switch to Auto Video Link Quality.");
   }
   if ( 1 == s_uLastQuickActionSwitchVideoProfile )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_MED, 0, NULL, 0);
      warnings_add(0, "Dev: Switch to Med Video Link Quality.");
   }
   if ( 2 == s_uLastQuickActionSwitchVideoProfile )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_LOW, 0, NULL, 0);
      warnings_add(0, "Dev: Switch to Low Video Link Quality.");
   }
}

void executeQuickActionCycleOSD()
{
   //if ( ! quickActionCheckVehicle("cycle the OSD screens") )
   //   return;

   g_bHasVideoDecodeStatsSnapshot = false;

   Model* pModel = osd_get_current_layout_source_model();
   if ( NULL == pModel )
      return;
   log_line("Execute quick action to switch OSD screen for VID %u (%s): from layout %d to next one", pModel->vehicle_id, pModel->getShortName(), pModel->osd_params.layout);

   int curentLayout = pModel->osd_params.layout;
   int k=0; 
   while ( k < 10 )
   {
      k++;
      pModel->osd_params.layout++;
      if ( pModel->osd_params.layout >= osdLayoutLast )
         pModel->osd_params.layout = osdLayout1;
      if ( pModel->osd_params.osd_flags2[pModel->osd_params.layout] & OSD_FLAG2_LAYOUT_ENABLED )
         break; 
   }

   if ( curentLayout == pModel->osd_params.layout )
   {
      char szBuff[128];
      sprintf(szBuff, "You have a single OSD screen enabled on %s. Enable more to be able to switch them.", pModel->getLongName());
      Popup* p = new Popup(szBuff, 0.1,0.7, 0.54, 4);
      p->setCentered();
      p->setIconId(g_idIconInfo, get_Color_IconWarning());
      popups_add_topmost(p);
      return;
   }

   osd_set_current_layout_index_and_source_model(pModel, pModel->osd_params.layout);

   u32 scale = pModel->osd_params.osd_preferences[pModel->osd_params.layout] & 0xFF;
   osd_setScaleOSD((int)scale);
   scale = (pModel->osd_params.osd_preferences[pModel->osd_params.layout]>>16) & 0x0F;
   osd_setScaleOSDStats((int)scale);
   osd_apply_preferences();
   applyFontScaleChanges();

   saveControllerModel(pModel);
   save_Preferences();

   if ( pModel->osd_params.layout == 0 )
      warnings_add(pModel->vehicle_id, "OSD Screen changed to Screen 1");
   if ( pModel->osd_params.layout == 1 )
      warnings_add(pModel->vehicle_id, "OSD Screen changed to Screen 2");
   if ( pModel->osd_params.layout == 2 )
      warnings_add(pModel->vehicle_id, "OSD Screen changed to Screen 3");
   if ( pModel->osd_params.layout == 3 )
      warnings_add(pModel->vehicle_id, "OSD Screen changed to Screen Lean");
   if ( pModel->osd_params.layout == 4 )
      warnings_add(pModel->vehicle_id, "OSD Screen changed to Screen Lean Extended");

   if ( pModel->is_spectator )
      return;

   //osd_parameters_t params;
   //memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   handle_commands_abandon_command();
   //handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t));
   handle_commands_send_single_oneway_command_to_vehicle(pModel->vehicle_id, 1, COMMAND_ID_SET_OSD_CURRENT_LAYOUT, (u32)pModel->osd_params.layout, NULL, 0, 0);
   g_iMustSendCurrentActiveOSDLayoutCounter = 10; // send it 10 times, every 200 ms
   g_TimeLastSentCurrentActiveOSDLayout = g_TimeNow;

   send_model_changed_message_to_router(MODEL_CHANGED_OSD_PARAMS, 0);
}

void executeQuickActionRelaySwitch()
{
   if ( g_pCurrentModel->is_spectator )
   {
      warnings_add(0, "Can't execute relay switching while in spectator mode.");
      return;
   }
   if ( ! quickActionCheckVehicle("relay switch") )
      return;
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
      return;
   
   s_uTimeLastQuickActionPress = get_current_timestamp_ms();

   if ( ! link_is_vehicle_online_now(g_pCurrentModel->vehicle_id) )
   {
      Popup* p =new Popup("You must be connected to a vehicle to switch relaying!", 0.1,0.8, 0.54, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0 )
   {
      char szBuff[128];
      sprintf(szBuff, "Relaying is not enabled on this vehicle (%s)!", g_pCurrentModel->getLongName() );
      Popup* p = new Popup(szBuff, 0.1,0.8, 0.5, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   // Switching to remote video stream?
   if ( ! (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) )
   if ( ! link_is_relayed_vehicle_online() )
   {
      Popup* p = new Popup("Relayed vehicle is not online. Can't switch to relay it.", 0.1,0.8, 0.54, 4);
      p->setCentered();
      p->setBottomAlign(true);
      p->setBottomMargin(0.2);
      p->setIconId(g_idIconInfo, get_Color_IconWarning());
      popups_add_topmost(p);
      return;
   }
   
   type_relay_parameters newParams;
   memcpy((u8*)&newParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   u32 uOldRelayMode = newParams.uCurrentRelayMode;

   // Switch to relayed vehicle
   if ( newParams.uCurrentRelayMode & RELAY_MODE_MAIN )
   {
      newParams.uCurrentRelayMode &= ~RELAY_MODE_MAIN;
      newParams.uCurrentRelayMode |= RELAY_MODE_REMOTE;

      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->relay_params.uRelayedVehicleId )
         {
            osd_set_current_data_source_vehicle_index(i);
            break;
         }
      }
   }
   // Switch to main vehicle
   else
   {
      newParams.uCurrentRelayMode &= ~RELAY_MODE_REMOTE;
      newParams.uCurrentRelayMode |= RELAY_MODE_MAIN;

      osd_set_current_data_source_vehicle_index(0);
   }

   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_SWITCH_OSD )
   {
      Model * pModel = g_pCurrentModel;
      if ( newParams.uCurrentRelayMode & RELAY_MODE_REMOTE )
         pModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 50);
        
      if ( NULL != pModel )
      {
         osd_set_current_layout_index_and_source_model(pModel, pModel->osd_params.layout);
         u32 scale = pModel->osd_params.osd_preferences[pModel->osd_params.layout] & 0xFF;
         osd_setScaleOSD((int)scale);
         scale = (pModel->osd_params.osd_preferences[pModel->osd_params.layout]>>16) & 0x0F;
         osd_setScaleOSDStats((int)scale);
         if ( render_engine_is_raw() )
            applyFontScaleChanges();
      }
   }

   newParams.uCurrentRelayMode |= RELAY_MODE_IS_RELAY_NODE;

   log_line("Pressed QA button to switch relay mode from %d to %d (%s to %s)",
      uOldRelayMode, newParams.uCurrentRelayMode,
      str_format_relay_mode(uOldRelayMode), str_format_relay_mode(newParams.uCurrentRelayMode));

   handle_commands_abandon_command();
   handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&newParams, sizeof(type_relay_parameters));
}

void executeQuickActionSwitchFavoriteVehicle()
{
   if ( ! vehicle_is_favorite(g_pCurrentModel->vehicle_id) )
   {
      warnings_add(0, "This vehicle is not a favorite vehicle. Add it to favorites to be able to cycle through favorites.");
      return;
   }
   u32 uOldVehicleId = g_pCurrentModel->vehicle_id;
   u32 uVehicleId = get_next_favorite(g_pCurrentModel->vehicle_id);
   Model* pNewVehicle = findModelWithId(uVehicleId, 51);
   if ( NULL == pNewVehicle )
   {
      warnings_add(0, "Favorite vehicle does not exist. Check your favorites vehicles list.");
      return;
   }

   if ( uVehicleId == g_pCurrentModel->vehicle_id )
   {
      warnings_add(0, "You have a single favorite vehicle. Add at least two in order to be able to cycle through favorites.");
      return;
   }

   log_line("--------------------------------------------------------------------------------------------");
   log_line("Starting QA favorite action to switch current vehicle from VID %u to %u...", uOldVehicleId, uVehicleId);

   g_bSwitchingFavoriteVehicle = true;
   Model* pOldVehicle = g_pCurrentModel;

   setCurrentModel(pNewVehicle->vehicle_id);
   g_pCurrentModel = getCurrentModel();

   setControllerCurrentModel(g_pCurrentModel->vehicle_id);
   saveControllerModel(g_pCurrentModel);

   ruby_set_active_model_id(g_pCurrentModel->vehicle_id);
   
   onMainVehicleChanged(false);

   //shared_vars_state_reset_all_vehicles_runtime_info();
   //g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->vehicle_id;
   //g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
   //g_iCurrentActiveVehicleRuntimeInfoIndex = 0;
   
   int iIndexRuntime = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->vehicle_id )
      {
         iIndexRuntime = i;
         break;
      }
   }
   if ( -1 == iIndexRuntime )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_VehiclesRuntimeInfo[i].uVehicleId == 0 )
         {
            iIndexRuntime = i;
            break;
         }
      }
   }
   if ( -1 == iIndexRuntime )
      iIndexRuntime = MAX_CONCURENT_VEHICLES-1;

   reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[iIndexRuntime]));
   g_VehiclesRuntimeInfo[iIndexRuntime].uVehicleId = g_pCurrentModel->vehicle_id;
   g_VehiclesRuntimeInfo[iIndexRuntime].pModel = g_pCurrentModel;

   t_structure_vehicle_info tmp;
   memcpy( &tmp, &(g_VehiclesRuntimeInfo[0]), sizeof(t_structure_vehicle_info));
   memcpy( &(g_VehiclesRuntimeInfo[0]), &(g_VehiclesRuntimeInfo[iIndexRuntime]), sizeof(t_structure_vehicle_info));
   memcpy( &(g_VehiclesRuntimeInfo[iIndexRuntime]), &tmp, sizeof(t_structure_vehicle_info));
   g_iCurrentActiveVehicleRuntimeInfoIndex = 0;

   log_line("Send message to router to switch favorite vehicle from VID %u [%s] to %u [%s]", uOldVehicleId, pOldVehicle->getShortName(), uVehicleId, pNewVehicle->getShortName());
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE, uVehicleId);
   char szBuff[256];
   sprintf(szBuff, "Switching to favorite vehicle: %s", pNewVehicle->getLongName());
   warnings_add(0, szBuff); 

   log_line("Finished QA favorite action to switch current vehicle from VID %u to %u", uOldVehicleId, uVehicleId);
   log_line("--------------------------------------------------------------------------------------------");
}