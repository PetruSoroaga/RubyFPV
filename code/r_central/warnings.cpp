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
#include "menu.h"
#include "osd.h"
#include "osd_common.h"
#include "shared_vars.h"

#include "ruby_central.h"
#include "popup_log.h"
#include "timers.h"

#define MAX_WARNINGS 8

Popup* s_PopupsWarnings[MAX_WARNINGS];
int s_PopupsWarningsCount = 0;
float s_fBottomYStartWarnings = 0.78;

u32 s_TimeLastWarningAdded = 0;
u32 s_TimeLastWarningRadioInitialized = 0;

u32 s_TimeLastWarningTooMuchData = 0;
u32 s_TimeLastWarningVehicleOverloaded = 0;

bool s_bIsLinkLostToControllerAlarm = false;
bool s_bIsLinkRecoveredToControllerAlarm = false;

void warnings_on_changed_vehicle()
{
   s_bIsLinkLostToControllerAlarm = false;
}

void warnings_remove_all()
{
   for( int i=0; i<s_PopupsWarningsCount; i++ )
   {
      popups_remove(s_PopupsWarnings[i]);
      delete s_PopupsWarnings[i];
   }
   s_PopupsWarningsCount = 0;
}

void warnings_add(const char* szTitle)
{
   warnings_add(szTitle, 0, NULL);
}

void warnings_add(const char* szTitle, u32 iconId)
{
   warnings_add(szTitle, iconId, NULL);
}

void warnings_add(const char* szTitle, u32 iconId, double* pColorIcon, int timeout)
{
   if ( NULL == szTitle || 0 == szTitle[0] )
      return;

   log_line("Adding warning popup (%d): %s", s_PopupsWarningsCount, szTitle);

   if ( s_PopupsWarningsCount > 0 )
   if ( NULL != szTitle )
   if ( 0 == strcmp(szTitle, s_PopupsWarnings[0]->getTitle()) )
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

   float dySpacing = 0.012*menu_getScaleMenus();

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
   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      xPos += osd_getVerticalBarWidth();

   Popup* p = new Popup(szTitle, xPos, s_fBottomYStartWarnings, 0.7, timeout);
   p->setFont(g_idFontOSDWarnings);

   //if ( pP->iPersistentMessages )
   //   p->setNoBackground();
   p->setBackgroundAlpha(0.25);
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

   log_line("Added warning popup: %s", szTitle);
   popup_log_add_entry(szTitle);
}

void warnings_add_error_null_model(int code)
{
   char szBuff[128];
   snprintf(szBuff, sizeof(szBuff), "Fatal error: Invalid model! (code: %x)", code);
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

void warnings_add_too_much_data(int current_kbps, int max_kbps)
{
   if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
      return;

   if ( s_TimeLastWarningTooMuchData != 0 )
   if ( g_TimeNow < s_TimeLastWarningTooMuchData + 10000 )
      return;
   s_TimeLastWarningTooMuchData = g_TimeNow;
   char szBuff[512];
   snprintf(szBuff, sizeof(szBuff), "You are sending too much data (%d Mbps) over the radio link (max capacity %d Mbps). Lower your video bitrate or increase your radio data rate.", current_kbps/1000, max_kbps/1000);
   warnings_add(szBuff, g_idIconWarning);
}

void warnings_add_vehicle_overloaded()
{
   if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
      return;

   if ( s_TimeLastWarningVehicleOverloaded != 0 )
   if ( g_TimeNow < s_TimeLastWarningVehicleOverloaded + 10000 )
      return;
   s_TimeLastWarningVehicleOverloaded = g_TimeNow;
   warnings_add("Your vehicle is overloaded. Can't keep up processing data for sending (EC). Lower your video bitrate or EC.", g_idIconWarning);
}

void warnings_add_link_to_controller_lost()
{
   if ( s_bIsLinkLostToControllerAlarm )
      return;
   s_bIsLinkLostToControllerAlarm = true;
   s_bIsLinkRecoveredToControllerAlarm = false;
   warnings_add("Vehicle: Radio link to controller is lost", g_idIconRadio, get_Color_IconError());
}

void warnings_add_link_to_controller_recovered()
{
   if ( s_bIsLinkRecoveredToControllerAlarm )
      return;
   s_bIsLinkRecoveredToControllerAlarm = true;
   s_bIsLinkLostToControllerAlarm = false;
   warnings_add("Vehicle: Radio link to controller recovered.", g_idIconRadio, get_Color_IconSucces());
}
