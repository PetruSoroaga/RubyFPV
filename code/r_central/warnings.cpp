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

#include "../base/alarms.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiolink.h"
#include "warnings.h"
#include "popup.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu/menu.h"
#include "osd/osd.h"
#include "osd/osd_common.h"
#include "shared_vars.h"

#include "link_watch.h"
#include "ruby_central.h"
#include "popup_log.h"
#include "timers.h"

#define MAX_WARNINGS 8

Popup* s_PopupSwitchingRadioLink = NULL;

Popup* s_PopupConfigureRadioLink = NULL;
u32 s_uTimeLastPopupConfigureRadioLinkUpdate = 0;

Popup* s_PopupsWarnings[MAX_WARNINGS];
int s_PopupsWarningsCount = 0;
float s_fBottomYStartWarnings = 0.78;

u32 s_TimeLastWarningAdded = 0;
u32 s_TimeLastWarningRadioInitialized = 0;

u32 s_TimeLastWarningTooMuchData = 0;
u32 s_TimeLastWarningVehicleOverloaded = 0;

bool s_bIsLinkLostToControllerAlarm[MAX_CONCURENT_VEHICLES];
bool s_bIsLinkRecoveredToControllerAlarm[MAX_CONCURENT_VEHICLES];

void warnings_on_changed_vehicle()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      s_bIsLinkRecoveredToControllerAlarm[i] = false;
      s_bIsLinkLostToControllerAlarm[i] = false;
   }
   popups_remove(s_PopupConfigureRadioLink);
   s_PopupConfigureRadioLink = NULL;
}

void warnings_remove_all()
{
   for( int i=0; i<s_PopupsWarningsCount; i++ )
   {
      popups_remove(s_PopupsWarnings[i]);
      delete s_PopupsWarnings[i];
      s_PopupsWarnings[i] = NULL;
   }
   s_PopupsWarningsCount = 0;

   popups_remove(s_PopupSwitchingRadioLink);
   s_PopupSwitchingRadioLink = NULL;

   popups_remove(s_PopupConfigureRadioLink);
   s_PopupConfigureRadioLink = NULL;
}

void warnings_periodic_loop()
{
   if ( NULL != s_PopupSwitchingRadioLink )
   {
      if ( s_PopupSwitchingRadioLink->isExpired() )
      {
         popups_remove(s_PopupSwitchingRadioLink);
         s_PopupSwitchingRadioLink = NULL;
      }
   }

   if ( NULL != s_PopupConfigureRadioLink )
   {
      if ( s_PopupConfigureRadioLink->isExpired() )
      {
         popups_remove( s_PopupConfigureRadioLink );
         s_PopupConfigureRadioLink = NULL;
         link_reset_reconfiguring_radiolink();
      }
      else if ( (g_TimeNow >= s_uTimeLastPopupConfigureRadioLinkUpdate + 300) && (s_PopupConfigureRadioLink->getTimeout() > 3.0) )
      {
         int iLine = s_PopupConfigureRadioLink->getLinesCount()-1;
         char* szLine = s_PopupConfigureRadioLink->getLine(iLine);
         if ( NULL != szLine )
         {
            int iDots = 0;
            int iPos = strlen(szLine)-1;
            while ( (iPos > 0) && (szLine[iPos] == '.') )
            {
               iPos--;
               iDots++;
            }
            iDots++;
            if ( iDots > 4 )
            {
               char szBuff[256];
               strcpy(szBuff, szLine);
               szBuff[iPos+1] = 0;
               s_PopupConfigureRadioLink->setLine(iLine, szBuff);
            }
            else
            {
               char szBuff[256];
               strcpy(szBuff, szLine);
               strcat(szBuff, ".");
               s_PopupConfigureRadioLink->setLine(iLine, szBuff);
            }
         }
      }
   }
}

void warnings_add(u32 uVehicleId, const char* szTitle)
{
   warnings_add(uVehicleId, szTitle, 0, NULL);
}

void warnings_add(u32 uVehicleId, const char* szTitle, u32 iconId)
{
   warnings_add(uVehicleId, szTitle, iconId, NULL);
}

void warnings_add(u32 uVehicleId, const char* szTitle, u32 iconId, double* pColorIcon, int timeout)
{
   if ( (NULL == szTitle) || (0 == szTitle[0]) )
      return;

   log_line("Adding warning popup (%d) for VID: %u: %s", s_PopupsWarningsCount, uVehicleId, szTitle);

   char szComposedTitle[256];
   strncpy(szComposedTitle, szTitle, 254);

   int iRuntimeInfoIndex = -1;
   int iCountVehicles = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

      if ( g_VehiclesRuntimeInfo[i].uVehicleId != 0 )
         iCountVehicles++;
   }
   if ( (0 != uVehicleId) && (iRuntimeInfoIndex != -1) && (iCountVehicles > 1) )
   if ( NULL != g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel )
      sprintf(szComposedTitle, "%s: %s", g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->getShortName(), szTitle);

   if ( s_PopupsWarningsCount > 0 )
   if ( 0 == strcmp(szComposedTitle, s_PopupsWarnings[0]->getTitle()) )
   if ( s_TimeLastWarningAdded > g_TimeNow-2000 )
   {
      log_line("Warning popup not added due to too soon duplicate with last one.");
      return;
   }

   s_TimeLastWarningAdded = g_TimeNow;

   Preferences* pP = get_Preferences();
   if ( pP->iDebugShowFullRXStats )
      return;
   if ( pP->iPersistentMessages )
      timeout = 25;

   float dySpacing = 0.012;

   // No more room?
   if ( s_PopupsWarningsCount >= MAX_WARNINGS )
   {
      if ( s_PopupsWarnings[MAX_WARNINGS-1] != NULL )
      {
         popups_remove(s_PopupsWarnings[MAX_WARNINGS-1]);
         delete s_PopupsWarnings[MAX_WARNINGS-1];
         s_PopupsWarnings[MAX_WARNINGS-1] = NULL;
         s_PopupsWarningsCount--;
      }
   }

   // Add it to the bottom of screen, start of the array

   float xPos = 0.01 + osd_getMarginX();
   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      xPos += osd_getVerticalBarWidth();

   Popup* p = new Popup(szComposedTitle, xPos, s_fBottomYStartWarnings, 0.7, timeout);

   //if ( g_pRenderEngine->textWidth(g_idFontMenu, szComposedTitle) > 0.5 )
   //   p->setFont(g_idFontMenuSmall);
   //else
      p->setFont(g_idFontMenu);

   //if ( pP->iPersistentMessages )
   //   p->setNoBackground();
   p->setBackgroundAlpha(0.5);
   p->setCustomPadding(0.3);

   if ( 0 != iconId )
   {
      double* pColor = get_Color_IconNormal();
      if ( pColorIcon != NULL )
         pColor = pColorIcon;
      p->setIconId(iconId, pColor);
   }

   popups_add(p);
   p->invalidate();
   float fHeightPopup = p->getRenderHeight();
   p->m_yPos -= fHeightPopup;
   p->invalidate();


   // Move it all up

   if ( s_PopupsWarningsCount > 0 )
   for( int i=s_PopupsWarningsCount; i>0; i-- )
   {
      s_PopupsWarnings[i] = s_PopupsWarnings[i-1];
      s_PopupsWarnings[i]->m_yPos -= dySpacing + fHeightPopup;
      s_PopupsWarnings[i]->invalidate();
   }

   s_PopupsWarnings[0] = p;
   s_PopupsWarningsCount++;

   // Remove expired ones

   if ( s_PopupsWarningsCount > 0 )
   for( int i = s_PopupsWarningsCount-1; i>=0; i-- )
   {
      if ( s_PopupsWarnings[i]->isExpired() )
      {
         float hPopup = s_PopupsWarnings[i]->getRenderHeight();
         popups_remove(s_PopupsWarnings[i]);
         delete s_PopupsWarnings[i];
         s_PopupsWarnings[i] = NULL;
         // Move down all above the expired one
         for( int j=i; j<s_PopupsWarningsCount-1; j++ )
         {
            s_PopupsWarnings[j] = s_PopupsWarnings[j+1];
            s_PopupsWarnings[j]->m_yPos += dySpacing + hPopup;
            s_PopupsWarnings[j]->invalidate();
         }
         s_PopupsWarningsCount--;
      }
   }

   log_line("Added warning popup: %s", szComposedTitle);
   popup_log_add_entry(szComposedTitle);
}

void warnings_add_error_null_model(int code)
{
   char szBuff[128];
   sprintf(szBuff, "Fatal error: Invalid model! (code: %x)", code);
   popup_log_add_entry(szBuff);
   Popup* p = new Popup(szBuff, 0.34, 0.68, 5);
   p->setIconId(g_idIconError, get_Color_IconError());
   popups_add(p);
   render_all(g_TimeNow);
}

void warnings_add_radio_reinitialized()
{
   if ( g_TimeNow > s_TimeLastWarningRadioInitialized + 70000 )
   {
      log_line("Added warning radio initialized.");
      s_TimeLastWarningRadioInitialized = g_TimeNow;
      Popup* p = new Popup("Your vehicle radio modules have been reinitialized", 0.5, 0.5, 0.6, 10.0);
      p->setCentered();
      p->addLine("Your vehicle radio modules have been restarted due to a harware issue. Please check your wiring and power supply on the vehicle.");
      p->setFont(g_idFontOSD);
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);
   }
}

void warnings_add_too_much_data(u32 uVehicleId, int current_kbps, int max_kbps)
{
   if ( (g_pCurrentModel == NULL) || g_pCurrentModel->is_spectator )
      return;

   if ( s_TimeLastWarningTooMuchData != 0 )
   if ( g_TimeNow < s_TimeLastWarningTooMuchData + 10000 )
      return;
   s_TimeLastWarningTooMuchData = g_TimeNow;

   char szBuff[256];
   sprintf(szBuff, "You are sending too much data (%d Mbps) over the radio link (max capacity %d Mbps). Lower your video bitrate or increase your radio data rate.", current_kbps/1000, max_kbps/1000);
   warnings_add(uVehicleId, szBuff, g_idIconWarning);
}

void warnings_add_vehicle_overloaded(u32 uVehicleId)
{
   if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
      return;

   if ( s_TimeLastWarningVehicleOverloaded != 0 )
   if ( g_TimeNow < s_TimeLastWarningVehicleOverloaded + 10000 )
      return;
   s_TimeLastWarningVehicleOverloaded = g_TimeNow;
   warnings_add(uVehicleId, "Your vehicle is overloaded. Can't keep up processing data for sending (EC). Lower your video bitrate or EC.", g_idIconWarning);
}

void warnings_add_link_to_controller_lost(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;
   if ( -1 == iRuntimeInfoIndex )
      return;

   if ( s_bIsLinkLostToControllerAlarm[iRuntimeInfoIndex] )
      return;
   s_bIsLinkLostToControllerAlarm[iRuntimeInfoIndex] = true;
   s_bIsLinkRecoveredToControllerAlarm[iRuntimeInfoIndex] = false;
   warnings_add(uVehicleId, "Vehicle: Radio link to controller is lost", g_idIconRadio, get_Color_IconError());
}

void warnings_add_link_to_controller_recovered(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;
   if ( -1 == iRuntimeInfoIndex )
      return;

   if ( s_bIsLinkRecoveredToControllerAlarm[iRuntimeInfoIndex] )
      return;
   s_bIsLinkRecoveredToControllerAlarm[iRuntimeInfoIndex] = true;
   s_bIsLinkLostToControllerAlarm[iRuntimeInfoIndex] = false;
   warnings_add(uVehicleId, "Vehicle: Radio link to controller recovered.", g_idIconRadio, get_Color_IconSucces());
}

Popup* warnings_get_configure_radio_link_popup()
{
   return s_PopupConfigureRadioLink;
}

void warnings_add_configuring_radio_link(int iRadioLink, const char* szMessage)
{
   if ( NULL != s_PopupConfigureRadioLink )
      return;
   
   log_line("Add popup for starting configuring radio link %d", iRadioLink+1);
   char szBuff[128];
   sprintf(szBuff, "Configuring Radio Link %d", iRadioLink+1);
   s_PopupConfigureRadioLink = new Popup(true, szBuff, 50);
   if ( (NULL != szMessage) && (0 != szMessage[0]) )
      s_PopupConfigureRadioLink->addLine(szMessage);
   else
      s_PopupConfigureRadioLink->addLine("Configuring");
   s_PopupConfigureRadioLink->addLine("Processing");
   s_PopupConfigureRadioLink->setFont(g_idFontOSDSmall);
   s_PopupConfigureRadioLink->setIconId(g_idIconRadio, get_Color_MenuText());
   s_PopupConfigureRadioLink->setFixedWidth(0.24);

   popups_add_topmost(s_PopupConfigureRadioLink);
}

void warnings_add_configuring_radio_link_line(const char* szLine)
{
   if ( NULL == s_PopupConfigureRadioLink )
      return;
   if ( (NULL == szLine) || ( 0 == szLine[0] ) )
      return;

   s_PopupConfigureRadioLink->removeLastLine();
   s_PopupConfigureRadioLink->addLine(szLine);
   s_PopupConfigureRadioLink->addLine("Processing");
   log_line("Configure radio link popup: added line %d: [%s]", s_PopupConfigureRadioLink->getLinesCount()-1, szLine);
}

void warnings_remove_configuring_radio_link(bool bSucceeded)
{
   if ( NULL == s_PopupConfigureRadioLink )
   {
      g_bConfiguringRadioLink = false;
      g_iConfiguringRadioLinkIndex = -1;
      g_bConfiguringRadioLinkWaitVehicleReconfiguration = false;
      g_bConfiguringRadioLinkWaitControllerReconfiguration = false;
      return;
   }

   if ( NULL != s_PopupConfigureRadioLink )
   if ( s_PopupConfigureRadioLink->getTimeout() > 3.0 )
   {
      if ( bSucceeded )
      {
         s_PopupConfigureRadioLink->removeLastLine();
         s_PopupConfigureRadioLink->addLine("Completed");
         s_PopupConfigureRadioLink->setTimeout(1.8);
      }
      else
      {
         s_PopupConfigureRadioLink->removeLastLine();
         s_PopupConfigureRadioLink->addLine("Failed!");
         s_PopupConfigureRadioLink->setTimeout(3.2);
      }
   }
}

void warnings_add_switching_radio_link(int iVehicleRadioLinkId, u32 uNewFreqKhz)
{
   if ( NULL != s_PopupSwitchingRadioLink )
      popups_remove(s_PopupSwitchingRadioLink);

   char szBuff[256];
   sprintf(szBuff, "Switching to vehicle radio link %d (%s)", iVehicleRadioLinkId+1, str_format_frequency(uNewFreqKhz));
   s_PopupSwitchingRadioLink = new Popup(true, szBuff, 50);
   s_PopupSwitchingRadioLink->setFont(g_idFontMenuLarge);
   s_PopupSwitchingRadioLink->addLine("Please wait...");
   s_PopupSwitchingRadioLink->setIconId(g_idIconRadio, get_Color_MenuText());
   s_PopupSwitchingRadioLink->setFixedWidth(0.24);

   popups_add_topmost(s_PopupSwitchingRadioLink);
}


void warnings_remove_switching_radio_link(int iVehicleRadioLinkId, u32 uNewFreqKhz, bool bSucceeded)
{
   if ( NULL == s_PopupSwitchingRadioLink )
      return;

   if ( bSucceeded )
   {
      s_PopupSwitchingRadioLink->removeLastLine();
      char szBuff[256];
      sprintf(szBuff, "Switched to vehicle radio link %d (%s)", iVehicleRadioLinkId+1, str_format_frequency(uNewFreqKhz));
      s_PopupSwitchingRadioLink->setTitle(szBuff);
      s_PopupSwitchingRadioLink->setTimeout(2.5);
   }
   else
   {
      s_PopupSwitchingRadioLink->removeLastLine();
      s_PopupSwitchingRadioLink->addLine("Failed.");
      s_PopupSwitchingRadioLink->setTimeout(3.5);
   }
}

void warnings_add_input_device_added(char* szDeviceName)
{
   char szBuff[128];
   if ( (NULL == szDeviceName) || (0 == szDeviceName[0]) )
      strcpy(szBuff, "A new USB input device was detected");
   else
      sprintf(szBuff, "A new USB input device (%s) was detected", szDeviceName);

   Popup* pPopup = new Popup(szBuff, 0.6, 0.15, 0.38, 5.0);
   pPopup->setFont(g_idFontMenuSmall);
   popups_add_topmost(pPopup);
}

void warnings_add_input_device_removed(char* szDeviceName)
{
   char szBuff[128];
   if ( (NULL == szDeviceName) || (0 == szDeviceName[0]) )
      strcpy(szBuff, "A USB input device was removed");
   else
      sprintf(szBuff, "USB input device (%s) was removed", szDeviceName);

   Popup* pPopup = new Popup(szBuff, 0.6, 0.15, 0.38, 4.0);
   pPopup->setFont(g_idFontMenuSmall);
   popups_add_topmost(pPopup);
}


void warnings_add_input_device_unknown_key(int iKeyCode)
{
   char szBuff[64];
   sprintf(szBuff, "Unkown key code %d", iKeyCode);
   Popup* pPopup = new Popup(szBuff, 0.8, 0.15, 0.18, 3.0);
   pPopup->setFont(g_idFontMenuSmall);
   popups_add_topmost(pPopup);
}