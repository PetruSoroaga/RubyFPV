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

#include "menu.h"
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_system_dev_stats.h"
#include "menu_system_dev_logs.h"
#include "menu_system_video_profiles.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../../radio/radiolink.h"
#include "../../base/utils.h"
#include "../rx_scope.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuSystemDevStats::MenuSystemDevStats(void)
:Menu(MENU_ID_SYSTEM_DEV_STATS, "Developer Stats Windows", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
   
   m_pItemsSelect[0] = new MenuItemSelect("Show Retransmissions Stats", "Shows the extended developer video retransmissions stats.");
   m_pItemsSelect[0]->addSelection("Off");
   m_pItemsSelect[0]->addSelection("On");
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexDevStatsVideo = addMenuItem(m_pItemsSelect[0]);


   m_pItemsSelect[1] = new MenuItemSelect("Show Extended Radio Stats", "Shows the extended developer radio stats.");
   m_pItemsSelect[1]->addSelection("Off");
   m_pItemsSelect[1]->addSelection("On");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexDevStatsRadio = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Show Full Radio RX Stats", "Shows full statistics about radio stack state.");
   m_pItemsSelect[2]->addSelection("Off");
   m_pItemsSelect[2]->addSelection("On");
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexDevFullRXStats = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[7] = new MenuItemSelect("Show Controller Adaptive Video Info Stats", "");
   m_pItemsSelect[7]->addSelection("Off");
   m_pItemsSelect[7]->addSelection("On");
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_IndexShowControllerAdaptiveInfoStats = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[3] = new MenuItemSelect("Show Vehicle Tx Stats", "Shows graphs about vehicle transmissions.");
   m_pItemsSelect[3]->addSelection("Off");
   m_pItemsSelect[3]->addSelection("On");
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexDevStatsVehicleTx = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect("Show Video Bitrate History", "Shows history graph on video bitrate and radio datarates.");
   m_pItemsSelect[4]->addSelection("Off");
   m_pItemsSelect[4]->addSelection("On");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexDevVehicleVideoBitrateHistory = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect("Show Vehicle's Video Link Stats", "Shows vehicle statistics (generated on the vehicle side) about video link params.");
   m_pItemsSelect[5]->addSelection("Off");
   m_pItemsSelect[5]->addSelection("On");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexDevVehicleVideoStreamStats = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[6] = new MenuItemSelect("Show Vehicle's Video Link Stats Graphs", "Shows vehicle graphs (generated on the vehicle side) about video link params.");
   m_pItemsSelect[6]->addSelection("Off");
   m_pItemsSelect[6]->addSelection("On");
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexDevVehicleVideoGraphs = addMenuItem(m_pItemsSelect[6]);

   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->setTextColor(get_Color_Dev());
}

void MenuSystemDevStats::valuesToUI()
{
   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   int layoutIndex = 0;
   if ( NULL != g_pCurrentModel )
      layoutIndex = g_pCurrentModel->osd_params.layout;

   m_pItemsSelect[0]->setSelectedIndex(pP->iDebugShowDevVideoStats);
   m_pItemsSelect[1]->setSelectedIndex(pP->iDebugShowDevRadioStats);
   m_pItemsSelect[2]->setSelectedIndex(pP->iDebugShowFullRXStats);
   
   m_pItemsSelect[3]->setSelectedIndex(0);
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP )
      m_pItemsSelect[3]->setSelectedIndex(1);

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY )
      m_pItemsSelect[4]->setSelectedIndex(1);

   m_pItemsSelect[5]->setSelectedIndex(pP->iDebugShowVehicleVideoStats);
   m_pItemsSelect[6]->setSelectedIndex(pP->iDebugShowVehicleVideoGraphs);
   m_pItemsSelect[7]->setSelectedIndex(0);
   if ( NULL != g_pCurrentModel )
   {
      m_pItemsSelect[7]->setEnabled(true);
      if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO )
         m_pItemsSelect[7]->setSelectedIndex(1);
      else
         m_pItemsSelect[7]->setSelectedIndex(0);
   }
   else
      m_pItemsSelect[7]->setEnabled(false);
}

void MenuSystemDevStats::onShow()
{
   Menu::onShow();
}

void MenuSystemDevStats::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuSystemDevStats::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
   m_iConfirmationId = 0;
}


void MenuSystemDevStats::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_IndexDevStatsVideo == m_SelectedIndex )
   {
      pP->iDebugShowDevVideoStats = m_pItemsSelect[0]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexDevStatsRadio == m_SelectedIndex )
   {
      pP->iDebugShowDevRadioStats = m_pItemsSelect[1]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexDevFullRXStats == m_SelectedIndex )
   {
      pP->iDebugShowFullRXStats = m_pItemsSelect[2]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexShowControllerAdaptiveInfoStats == m_SelectedIndex )
   if ( NULL != g_pCurrentModel )
   {
      osd_parameters_t params;
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      int layoutIndex = g_pCurrentModel->osd_params.layout;

      params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      if ( 1 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
   
   if ( m_IndexDevStatsVehicleTx == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP;
      if ( ! handle_commands_send_developer_flags() )
         valuesToUI();  
      return;    
   }

   if ( m_IndexDevVehicleVideoBitrateHistory == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      osd_parameters_t params;
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      int layoutIndex = g_pCurrentModel->osd_params.layout;

      params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY;
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
   }

   if ( m_IndexDevVehicleVideoStreamStats == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      pP->iDebugShowVehicleVideoStats = m_pItemsSelect[5]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
   }

   if ( m_IndexDevVehicleVideoGraphs == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      pP->iDebugShowVehicleVideoGraphs = m_pItemsSelect[6]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
   }
}

