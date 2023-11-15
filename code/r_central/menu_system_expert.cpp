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

#include "../base/base.h"
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_system_expert.h"
#include "menu_system_dev_logs.h"
#include "menu_system_video_profiles.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_system_dev_stats.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../radio/radiolink.h"

#include "colors.h"
#include "pairing.h"
#include "../base/utils.h"
#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"
#include "rx_scope.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuSystemExpert::MenuSystemExpert(void)
:Menu(MENU_ID_SYSTEM_EXPERT, "Developer Settings", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
   m_ExtraHeight += 0.5*menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS*(1+MENU_TEXTLINE_SPACING);

   float fSliderWidth = 0.14;

   m_IndexLogs = -1;
   //m_IndexLogs = addMenuItem( new MenuItem("Logs Settings") );
   //m_pMenuItems[m_IndexLogs]->showArrow();

   m_IndexDevStats = addMenuItem( new MenuItem("Dev Stats Windows") );
   m_pMenuItems[m_IndexDevStats]->showArrow();

   addMenuItem(new MenuItemSection("Video and Radio Link"));

   m_IndexVideoProfiles = addMenuItem(new MenuItem("Video Link Profiles", "Change video link profiles and params."));
   m_pMenuItems[m_IndexVideoProfiles]->showArrow();

   m_pItemsSlider[0] = new MenuItemSlider("Max Radio Packet Size", "Maximum size in bytes that can be set for a radio packet in the user interface.", 100,1500,1250, fSliderWidth);
   m_pItemsSlider[0]->setStep(10);
   m_IndexPacket = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[0] = new MenuItemSelect("Clock Sync Type", "How the clocks between vehicle and controller are synchronized.");
   m_pItemsSelect[0]->addSelection("None");
   m_pItemsSelect[0]->addSelection("Basic");
   m_pItemsSelect[0]->addSelection("Advanced");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexClockSync = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[7] = new MenuItemSlider("Ping/Clock Sync Frequency", "How often is the clock sync done with the vehicle.", 1,50,10, fSliderWidth);
   m_pItemsSlider[7]->setStep(1);
   m_IndexPingClockSpeed = addMenuItem(m_pItemsSlider[7]);

   m_pItemsSelect[5] = new MenuItemSelect("Video Link Overload Check", "Continously monitor the video link and if the video link is overloaded (takes too much time to transmit or has too big video bitrate for current radio datarate) will temporarly reduce the video bitrate.");
   m_pItemsSelect[5]->addSelection("Off");
   m_pItemsSelect[5]->addSelection("On");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexVideoOverload = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSlider[8] = new MenuItemSlider("Radio Reconfiguration Delay (ms)", "When 2.4/5.8Ghz radio interfaces need to be reconfigured, allow a delay for reconfiguration. Important for Atheros chipset cards mostly (in miliseconds).", 1,100,10, fSliderWidth);
   m_pItemsSlider[8]->setStep(1);
   m_IndexWiFiChangeDelay = addMenuItem(m_pItemsSlider[8]);

   m_pItemsSelect[1] = new MenuItemSelect("Radio Silence Failsafe", "Restarts the vehicle if there is radio silence for more than 1 minute.");
   m_pItemsSelect[1]->addSelection("Disabled");
   m_pItemsSelect[1]->addSelection("Enabled");
   m_IndexRadioSilence = addMenuItem(m_pItemsSelect[1]);

   /*
   m_pItemsSelect[4] = new MenuItemSelect("Radio CTS protection", "Enable CTS protection on the radio frames.");  
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexFrameType = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSlider[3] = new MenuItemSlider("Radio SlotTime", "Sets the 'slottime' parameter on the radio stack (only for Atheros chipsets).", 1,60,0, fSliderWidth);
   m_IndexSlotTime = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSlider[4] = new MenuItemSlider("Radio Thresh62", "Sets the 'thresh62' parameter on the radio stack (only for Atheros chipsets).", 1,60,0, fSliderWidth);
   m_IndexThresh62 = addMenuItem(m_pItemsSlider[4]);
   */

   m_IndexFrameType = -1;
   m_IndexSlotTime = -1;
   m_IndexThresh62 = -1;

   addMenuItem(new MenuItemSection("Dev Tools"));

   m_pItemsSelect[3] = new MenuItemSelect("OSD Render FPS", "How often should the OSD be drawn.");
   m_pItemsSelect[3]->addSelection("10");
   m_pItemsSelect[3]->addSelection("15");
   m_pItemsSelect[3]->addSelection("20");
   m_pItemsSelect[3]->addSelection("25");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexRenderFSP = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[13] = new MenuItemSelect("Show UI/OSD CPU Usage", "Shows the CPU resources used by the UI and OSD interface.");
   m_pItemsSelect[13]->addSelection("No");
   m_pItemsSelect[13]->addSelection("Yes");
   m_pItemsSelect[13]->setUseMultiViewLayout();
   m_IndexCPULoad = addMenuItem(m_pItemsSelect[13]);

   m_pItemsSelect[14] = new MenuItemSelect("Pause OSD", "Pause/Resume OSD using the [Cancel]/[Back] button.");
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setUseMultiViewLayout();
   m_IndexFreezeOSD = addMenuItem(m_pItemsSelect[14]);

   m_pItemsSelect[15] = new MenuItemSelect("Inject recoverable video tx faults", "Inject video tx faults: packets that can't be transmitted but could be retransmitted.");
   m_pItemsSelect[15]->addSelection("No");
   m_pItemsSelect[15]->addSelection("Yes");
   m_pItemsSelect[15]->setUseMultiViewLayout();
   m_IndexInjectMinorFaults = addMenuItem(m_pItemsSelect[15]);

   m_pItemsSelect[12] = new MenuItemSelect("Inject unrecoverable video tx faults", "Inject video tx faults: packets that can't be transmitted and packets that can't be retransmitted too.");
   m_pItemsSelect[12]->addSelection("No");
   m_pItemsSelect[12]->addSelection("Yes");
   m_pItemsSelect[12]->setUseMultiViewLayout();
   m_IndexInjectFaults = addMenuItem(m_pItemsSelect[12]);


   m_IndexVersion = addMenuItem(new MenuItem("Modules versions", "Get all modules versions."));

   m_IndexResetDev = addMenuItem(new MenuItem("Reset Developer Settings", "Resets all the developer settings to the factory default values."));

   m_IndexDetailedPackets = addMenuItem( new MenuItem("RX/TX Scope") );
   //m_IndexRXScope = addMenuItem( new MenuItem("RX Scope") );

   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->setTextColor(get_Color_Dev());
}

void MenuSystemExpert::valuesToUI()
{

   m_ExtraHeight = menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   m_pItemsSlider[0]->setCurrentValue(pP->iDebugMaxPacketSize);
   m_pItemsSlider[7]->setCurrentValue(pCS->nPingClockSyncFrequency);
   m_pItemsSlider[8]->setCurrentValue(pP->iDebugWiFiChangeDelay);

   if ( NULL == g_pCurrentModel )
   {
      m_pItemsSelect[0]->setSelection(0);
      m_pItemsSelect[0]->setEnabled(false);
      m_pItemsSelect[1]->setSelection(0);
      m_pItemsSelect[1]->setEnabled(false);

      m_pItemsSelect[5]->setEnabled(false);

      //m_pItemsSelect[4]->setSelection(1);
      //m_pItemsSlider[3]->setCurrentValue(1);
      //m_pItemsSlider[4]->setCurrentValue(1);

      m_pItemsSelect[12]->setEnabled(false);
      m_pItemsSelect[15]->setEnabled(false);
      m_pMenuItems[m_IndexResetDev]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[0]->setEnabled(true);
      m_pItemsSelect[0]->setSelection( g_pCurrentModel->clock_sync_type );
      m_pItemsSelect[1]->setEnabled(true);
      m_pItemsSelect[1]->setSelection( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?1:0 );

      m_pItemsSelect[5]->setEnabled(true);
      m_pItemsSelect[5]->setSelection( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_DISABLE_VIDEO_OVERLOAD_CHECK)?0:1 );

      //if ( g_pCurrentModel->nic_radio_flags[0] & RADIO_FLAGS_FRAME_TYPE_RTS )
      //   m_pItemsSelect[4]->setSelection(0);
      //else
      //   m_pItemsSelect[4]->setSelection(1);
      //m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->slotTime);
      //m_pItemsSlider[4]->setCurrentValue(g_pCurrentModel->thresh62);

      m_pItemsSelect[12]->setEnabled(true);
      m_pItemsSelect[12]->setSelectedIndex(0);
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS )
         m_pItemsSelect[12]->setSelectedIndex(1);
      m_pItemsSelect[15]->setEnabled(true);
      m_pItemsSelect[15]->setSelectedIndex(0);
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS )
         m_pItemsSelect[15]->setSelectedIndex(1);

   }

   m_pItemsSelect[3]->setSelection( (pCS->iRenderFPS-10)/5 );
   m_pItemsSelect[13]->setSelectedIndex(pP->iShowCPULoad);
   m_pItemsSelect[14]->setSelectedIndex(pCS->iFreezeOSD);
}

void MenuSystemExpert::onShow()
{
   Menu::onShow();
}


void MenuSystemExpert::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuSystemExpert::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 1 == m_iConfirmationId && 1 == returnValue )
   {
   }
   if ( 20 == m_iConfirmationId && 1 == returnValue )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS, 0, NULL, 0) )
      {
         valuesToUI();
         return;
      }

      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iDisableRetransmissionsAfterControllerLinkLostMiliseconds = DEFAULT_CONTROLLER_LINK_MILISECONDS_TIMEOUT_TO_DISABLE_RETRANSMISSIONS;
      pCS->nRetryRetransmissionAfterTimeoutMS = DEFAULT_VIDEO_RETRANS_RETRY_TIME;
      pCS->nRequestRetransmissionsOnVideoSilenceMs = DEFAULT_VIDEO_RETRANS_REQUEST_ON_VIDEO_SILENCE_MS;
      save_ControllerSettings();      

      m_iConfirmationId = 21;
      Menu* pm = new MenuConfirmation("Developer Settings Reset", "Vehicle and controller will reboot now.", m_iConfirmationId, true);
      pm->m_yPos = 0.4;
      add_menu_to_stack(pm);
      return;
   }

   if ( 21 == m_iConfirmationId )
   {
      hw_execute_bash_command("sudo reboot -f", NULL);      
   }
   m_iConfirmationId = 0;
}


void MenuSystemExpert::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_IndexDevStats == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemDevStats());
      return;
   }

   if ( m_IndexPacket == m_SelectedIndex )
   {
      int val = m_pItemsSlider[0]->getCurrentValue();
      pP->iDebugMaxPacketSize = val;
      save_Preferences();
   }    

   if ( m_IndexClockSync == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      int clockType = m_pItemsSelect[0]->getSelectedIndex();
      if ( clockType != g_pCurrentModel->clock_sync_type )
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CLOCK_SYNC_TYPE, clockType, NULL, 0) )
         valuesToUI();
   }

   if ( m_IndexPingClockSpeed == m_SelectedIndex )
   {
      pCS->nPingClockSyncFrequency = m_pItemsSlider[7]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      valuesToUI();
   }

   if ( m_IndexRenderFSP == m_SelectedIndex )
   {
      pCS->iRenderFPS = 10 + m_pItemsSelect[3]->getSelectedIndex()*5;
      save_ControllerSettings();
      valuesToUI();
   }

   if ( m_IndexWiFiChangeDelay == m_SelectedIndex )
   {
      pP->iDebugWiFiChangeDelay = m_pItemsSlider[8]->getCurrentValue();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexRadioSilence == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      u32 enable = m_pItemsSelect[1]->getSelectedIndex();
      if ( 0 == enable )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI();  
   }

   if ( m_IndexFrameType == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessage("Radio CTS protection can't be changed. Connect to a vehicle to change it.");
         return;
      }
      
      return;
   }


   if ( m_IndexSlotTime == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessage("Radio SlotTime can't be changed. Connect to a vehicle to change it.");
         return;
      }
      int val = m_pItemsSlider[3]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_SLOTTIME, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexThresh62 == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessage("Radio Thresh62 can't be changed. Connect to a vehicle to change it.");
         return;
      }
      int val = m_pItemsSlider[4]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_THRESH62, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexVideoProfiles == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemVideoProfiles());
      return;
   }

   if ( -1 != m_IndexLogs )
   if ( m_IndexLogs == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemDevLogs());
      return;
   }

   if ( m_IndexDetailedPackets == m_SelectedIndex )
   {
      t_packet_header PH;

      if ( g_bIsRouterPacketsHistoryGraphOn )
      {
         g_bIsRouterPacketsHistoryGraphOn = false;
         g_bIsRouterPacketsHistoryGraphPaused = false;
         PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_STOP;
         handle_commands_send_ruby_message(&PH, NULL, 0);
         menu_close_all();
         shared_mem_router_packets_stats_history_close(g_pDebugSMRPST);
         return;
      }
      else
      {
         g_bIsRouterPacketsHistoryGraphOn = true;
         g_bIsRouterPacketsHistoryGraphPaused = false;
         PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_START;
         handle_commands_send_ruby_message(&PH, NULL, 0);
         hardware_sleep_ms(500);
         g_pDebugSMRPST = shared_mem_router_packets_stats_history_open_read();
         if ( NULL == g_pDebugSMRPST )
         {
            log_softerror_and_alarm("Failed to open shared mem for read for router packets stats history.");
            return;
         } 
         menu_close_all();
         return;
      }
   }

   if ( m_IndexCPULoad == m_SelectedIndex )
   {
      pP->iShowCPULoad = m_pItemsSelect[13]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexVideoOverload == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      u32 enable = m_pItemsSelect[5]->getSelectedIndex();
      if ( 1 == enable )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_DISABLE_VIDEO_OVERLOAD_CHECK);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_DISABLE_VIDEO_OVERLOAD_CHECK;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI();  
      return;
   }

   if ( m_IndexFreezeOSD == m_SelectedIndex )
   {
      pCS->iFreezeOSD = m_pItemsSelect[14]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexInjectFaults == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      int val = m_pItemsSelect[12]->getSelectedIndex();
      if ( 0 == val )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI();  
   }

   if ( m_IndexInjectMinorFaults == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      int val = m_pItemsSelect[15]->getSelectedIndex();
      if ( 0 == val )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI();  
   }

   if ( m_IndexVersion == m_SelectedIndex )
   {
      char szBuff[1024];
      char szOutput[1024];

      Menu* pMenu = new Menu(0,"All Modules Versions",NULL);
      pMenu->m_xPos = 0.32;
      pMenu->m_yPos = 0.17;
      pMenu->m_Width = 0.6;
      
      hw_execute_bash_command_raw_silent("./ruby_start -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_start: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_controller -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_controller: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_central -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_central: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rt_station -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_rt_station: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rx_telemetry -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_rx_telemetry: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_tx_rc -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_tx_rc: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_i2c -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_i2c: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_player_p -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_player_p: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_update_worker -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_update_worker: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_vehicle -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_vehicle: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rt_vehicle -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_rt_vehicle: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_tx_telemetry -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_tx_telemetry: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rx_rc -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      snprintf(szBuff, sizeof(szBuff), "ruby_rx_rc: %s", szOutput);
      pMenu->addTopLine(szBuff);

      add_menu_to_stack(pMenu);
      return;
   }

   if ( m_IndexResetDev == m_SelectedIndex )
   {
      m_iConfirmationId = 20;
      Menu* pm = new MenuConfirmation("Developer Settings Reset", "All developer settings where reset.", m_iConfirmationId);
      pm->m_yPos = 0.4;
      add_menu_to_stack(pm);
   }

   if ( m_IndexRXScope == m_SelectedIndex )
   {
      menu_close_all();
      rx_scope_start();
   }
}

