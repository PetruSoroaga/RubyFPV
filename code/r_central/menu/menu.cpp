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
#include "menu_root.h"
#include "menu_search.h"
#include "../../renderer/render_engine.h"
#include "osd_common.h"
#include "osd_ahi.h"
#include "osd.h"
#include "../../base/base.h"
#include "../../base/hardware.h"
#include "../../base/gpio.h"
#include "../../base/config.h"
#include "../../base/shared_mem_i2c.h"
#include "../../base/ctrl_settings.h"
#include "../../base/ctrl_interfaces.h"
#include "../popup_camera_params.h"
#include "../shared_vars.h"
#include "../timers.h"
#include "../ruby_central.h"
#include "../keyboard.h"

static int s_iCountRotaryEncoderCancelCount = 0;
static int s_iCountRotaryEncoder2CancelCount = 0;

Menu* gpMenuStack[MAX_MENU_STACK];
int g_iMenuStackTopIndex = 0;
static float s_fMenuGlobalAlpha = 1.0;

static u32 s_uTimeLastMenuRotaryEvent = 0;
static u16 s_uLastMenuRotaryEventIndex = 0;

static u32 s_uMenuLoopCounter = 0;

float menu_get_XStartPos(float fWidth)
{
   Preferences* pP = get_Preferences();

   // Stacked menus ?

   if ( pP->iMenusStacked )
   {
      if ( 0 == g_iMenuStackTopIndex )
         return 0.06;
      return 0.06 + g_iMenuStackTopIndex * 0.02;
   }

   // Non stacked menus (side by side)

   if ( 0 == g_iMenuStackTopIndex )
      return 0.06;

   //float dx = gpMenuStack[g_iMenuStackTopIndex-1]->m_Width*0.7;
   //if ( dx > 0.18 )
   //   dx = 0.18;
   float dx = 0.12;

   float xPos = gpMenuStack[g_iMenuStackTopIndex-1]->m_xPos + dx;
   if ( xPos + fWidth > 0.98 )
      xPos = 0.98 - fWidth;
   return xPos;
}

float menu_get_XStartPos(Menu* pMenu, float fWidth)
{

   float xPos = 0.1;
   if ( g_iMenuStackTopIndex > 0 )
      xPos = gpMenuStack[g_iMenuStackTopIndex-1]->m_xPos + gpMenuStack[g_iMenuStackTopIndex-1]->getRenderWidth()*0.5;
   return xPos;
}

float menu_setGlobalAlpha(float fAlpha)
{
   float f = s_fMenuGlobalAlpha;
   s_fMenuGlobalAlpha = fAlpha;
   return f;
}

float menu_getGlobalAlpha()
{
   return s_fMenuGlobalAlpha;
}

int menu_init()
{
   return 1;
}

void menu_close_all()
{
   for( int i=g_iMenuStackTopIndex-1; i>=0; i-- )
   {
      gpMenuStack[i]->setParent(NULL);
      delete gpMenuStack[i];
   }
   g_iMenuStackTopIndex = 0;
}

bool menu_has_menu_confirmation_above(Menu* pMenu)
{
   if ( NULL == pMenu )
      return false;

   int kIndex = -1;
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( gpMenuStack[i] == pMenu )
      {
         kIndex = i;
         break;
      }

   if ( kIndex < 0 )
      return false;

   kIndex++;
   if ( kIndex > 0 && kIndex < g_iMenuStackTopIndex && gpMenuStack[kIndex]->m_MenuId == MENU_ID_CONFIRMATION )
      return true;
   return false;
}

bool menu_has_menu(int menuId)
{
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( gpMenuStack[i]->m_MenuId == menuId )
         return true;
   return false;
}

Menu* menu_get_menu_by_id(int menuId)
{
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( gpMenuStack[i]->m_MenuId == menuId )
         return gpMenuStack[i];
   return NULL;
}

void add_menu_to_stack(Menu* pMenu)
{
   if ( NULL == pMenu )
      return;
   if ( g_iMenuStackTopIndex > 0 )
   {
      pMenu->setParent(gpMenuStack[g_iMenuStackTopIndex-1]);
      gpMenuStack[g_iMenuStackTopIndex-1]->onChildMenuAdd();
   }
   pMenu->m_MenuDepth = g_iMenuStackTopIndex;
   gpMenuStack[g_iMenuStackTopIndex] = pMenu;
   g_iMenuStackTopIndex++;
   pMenu->onShow();
}

void remove_menu_from_stack(Menu* pMenu)
{
   if ( NULL == pMenu )
      return;
   pMenu->setParent(NULL);
   int k = -1;
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( gpMenuStack[i] == pMenu )
      {
         k = i;
         break;
      }

   if ( k < 0 )
      return;
   while ( k < g_iMenuStackTopIndex-1 )
   {
      gpMenuStack[k] = gpMenuStack[k+1];
      k++;
   }
   g_iMenuStackTopIndex--;
   delete pMenu;
}

void replace_menu_on_stack(Menu* pMenuSrc, Menu* pMenuNew)
{
   if ( NULL == pMenuSrc || NULL == pMenuNew )
      return;

   pMenuSrc->setParent(NULL);
   int k = -1;
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( gpMenuStack[i] == pMenuSrc )
      {
         k = i;
         break;
      }

   if ( k < 0 )
      return;

   if ( k > 0 )
      pMenuNew->setParent(gpMenuStack[k-1]);
   pMenuNew->m_MenuDepth = pMenuSrc->m_MenuDepth;
   gpMenuStack[k] = pMenuNew;
   pMenuNew->onShow();
}

void menu_stack_pop(int returnValue)
{
   int iConfirmationId = -1;
   if ( g_iMenuStackTopIndex > 0 )
   {
      iConfirmationId = gpMenuStack[g_iMenuStackTopIndex-1]->getConfirmationId();
      gpMenuStack[g_iMenuStackTopIndex-1]->setParent(NULL);
      delete gpMenuStack[g_iMenuStackTopIndex-1];
      g_iMenuStackTopIndex--;
   }
   if ( g_iMenuStackTopIndex > 0 && -1 != iConfirmationId )
     gpMenuStack[g_iMenuStackTopIndex-1]->onReturnFromChild(returnValue);
}

void _menu_check_rotary_encoders_buttons( bool* pbSelect, bool* pbCancel, bool* pbRotatedCW, bool* pbRotatedCCW, bool* pbRotatedFastCW, bool* pbRotatedFastCCW, bool* pbSelect2, bool* pbCancel2, bool* pbRotatedCW2, bool* pbRotatedCCW2, bool* pbRotatedFastCW2, bool* pbRotatedFastCCW2)
{
   ControllerSettings* pCS = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( NULL == pCS || NULL == pCI )
      return;

   if ( ! hardware_i2c_has_external_extenders_rotary_encoders() )
   if ( ! hardware_i2c_has_external_extenders_buttons() )
   if ( ! hardware_i2c_HasPicoExtender() )
      return;

   if ( NULL == g_pSMRotaryEncoderButtonsEvents )
      g_pSMRotaryEncoderButtonsEvents = shared_mem_i2c_rotary_encoder_buttons_events_open_for_read();

   if ( NULL == g_pSMRotaryEncoderButtonsEvents )
      return;

   if ( s_uLastMenuRotaryEventIndex == g_pSMRotaryEncoderButtonsEvents->uEventIndex )
      return;

   if ( s_uTimeLastMenuRotaryEvent == g_pSMRotaryEncoderButtonsEvents->uTimeStamp )
      return;

   t_shared_mem_i2c_rotary_encoder_buttons_events events;
   memcpy(&events, g_pSMRotaryEncoderButtonsEvents, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) );
   u32 uCRC = base_compute_crc32((u8*)&events, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) - sizeof(u32));
   if ( uCRC != events.uCRC )
   {
      hardware_sleep_micros(200);
      memcpy(&events, g_pSMRotaryEncoderButtonsEvents, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) );
      u32 uCRC = base_compute_crc32((u8*)&events, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) - sizeof(u32));
      if ( uCRC != events.uCRC )
         return;
   }

   s_uTimeLastMenuRotaryEvent = events.uTimeStamp;
   s_uLastMenuRotaryEventIndex = events.uEventIndex;

   if ( ! events.uHasRotaryEncoder )
      s_iCountRotaryEncoderCancelCount = 0;

   if ( events.uHasRotaryEncoder )
   if ( (events.uRotaryEncoderEvents & 0x01) || (events.uRotaryEncoderEvents & (0x01<<2)) || (events.uRotaryEncoderEvents & (0x01<<3)) )
      s_iCountRotaryEncoderCancelCount = 0;

   if ( ! events.uHasSecondaryRotaryEncoder )
      s_iCountRotaryEncoder2CancelCount = 0;

   if ( events.uHasSecondaryRotaryEncoder )
   if ( (events.uRotaryEncoder2Events & 0x01) || (events.uRotaryEncoder2Events & (0x01<<2)) || (events.uRotaryEncoder2Events & (0x01<<3)) )
      s_iCountRotaryEncoder2CancelCount = 0;

   // Cancel rotary encoder (long press on rotary encoder) ?
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & 0x02 )
   {
      s_iCountRotaryEncoderCancelCount++;
      if ( ! events.uHasSecondaryRotaryEncoder )
      if ( s_iCountRotaryEncoderCancelCount > 7 )
      {
         pCS->nRotaryEncoderFunction = 1;
         save_ControllerSettings();
         s_iCountRotaryEncoderCancelCount = 0;
      }
      *pbCancel = true;
   }

   // Cancel rotary encoder 2 (long press on rotary encoder) ?
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & 0x02 )
   {
      s_iCountRotaryEncoder2CancelCount++;
      *pbCancel2 = true;
   }

   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & 0x01 )
   {
      *pbSelect = true;
      log_line("[Rotary1 Event] Select");
   }
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<2) )    
      *pbRotatedCCW = true;
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<3) )
      *pbRotatedCW = true;
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<4) )
      *pbRotatedFastCCW = true;
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<5) )
      *pbRotatedFastCW = true;

   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & 0x01 )
   {
      *pbSelect2 = true;
      log_line("[Rotary2 Event] Select");
   }
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<2) )
      *pbRotatedCCW2 = true;
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<3) )
      *pbRotatedCW2 = true;
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<4) )
      *pbRotatedFastCCW2 = true;
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<5) )
      *pbRotatedFastCW2 = true;

   if ( 0 != events.uButtonsEvents )
   {
      hardware_override_keys((events.uButtonsEvents & 0x01)?1:0, (events.uButtonsEvents & (0x01<<1))?1:0, (events.uButtonsEvents & (0x01<<2))?1:0, (events.uButtonsEvents & (0x01<<3))?1:0,
             ((events.uButtonsEvents>>16) & 0x01)?1:0, ((events.uButtonsEvents>>16) & (0x01<<1))?1:0, ((events.uButtonsEvents>>16) & (0x01<<2))?1:0, ((events.uButtonsEvents>>16) & (0x01<<3))?1:0);
   }
}


void menu_loop()
{
   s_uMenuLoopCounter++;

   ControllerSettings* pCS = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( g_iMenuStackTopIndex > 0 && NULL != gpMenuStack[g_iMenuStackTopIndex-1] )
   {
      if ( gpMenuStack[g_iMenuStackTopIndex-1]->m_MenuId != MENU_ID_CONFIRMATION )
         gpMenuStack[g_iMenuStackTopIndex-1]->periodicLoop();
      else
      {
         if ( g_iMenuStackTopIndex > 1 && NULL != gpMenuStack[g_iMenuStackTopIndex-2] )
            gpMenuStack[g_iMenuStackTopIndex-2]->periodicLoop();
      }
   }

   bool bRotarySelect = false;
   bool bRotaryCancel = false;
   bool bRotaryRotatedCW = false;
   bool bRotaryRotatedCCW = false;
   bool bRotaryRotatedFastCW = false;
   bool bRotaryRotatedFastCCW = false;
   bool bRotaryEvents = false;

   bool bRotary2Select = false;
   bool bRotary2Cancel = false;
   bool bRotary2RotatedCW = false;
   bool bRotary2RotatedCCW = false;
   bool bRotary2RotatedFastCW = false;
   bool bRotary2RotatedFastCCW = false;
   bool bRotary2Events = false;
   
   _menu_check_rotary_encoders_buttons(&bRotarySelect, &bRotaryCancel, &bRotaryRotatedCW, &bRotaryRotatedCCW, &bRotaryRotatedFastCW, &bRotaryRotatedFastCCW, &bRotary2Select, &bRotary2Cancel, &bRotary2RotatedCW, &bRotary2RotatedCCW, &bRotary2RotatedFastCW, &bRotary2RotatedFastCCW);

   bool bRotaryRotated = bRotaryRotatedCW | bRotaryRotatedCCW | bRotaryRotatedFastCW | bRotaryRotatedFastCCW;
   bool bRotary2Rotated = bRotary2RotatedCW | bRotary2RotatedCCW | bRotary2RotatedFastCW | bRotary2RotatedFastCCW;

   if ( pCS->nRotaryEncoderSpeed == 1 )
   {
      bRotaryRotatedFastCW = false;
      bRotaryRotatedFastCCW = false;
   }

   if ( pCS->nRotaryEncoderSpeed2 == 1 )
   {
      bRotary2RotatedFastCW = false;
      bRotary2RotatedFastCCW = false;
   }

   if ( (NULL != pCS) && (pCS->nRotaryEncoderFunction == 2) && (s_StartSequence == START_SEQ_COMPLETED) )
   {
      if ( (bRotarySelect || bRotaryRotatedCW || bRotaryRotatedCCW) && (!popups_has_popup(g_pPopupCameraParams)) )
      {
         if ( NULL == g_pPopupCameraParams )
            g_pPopupCameraParams = new PopupCameraParams();
         popups_add_topmost(g_pPopupCameraParams);
         g_pPopupCameraParams->invalidate();
         bRotarySelect = false;
      }
      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
         g_pPopupCameraParams->handleRotaryEvents(bRotaryRotatedCW, bRotaryRotatedCCW, bRotaryRotatedFastCW, bRotaryRotatedFastCCW, bRotarySelect, bRotaryCancel);
   }

   if ( (NULL != pCS) && (pCS->nRotaryEncoderFunction2 == 2) && (s_StartSequence == START_SEQ_COMPLETED) )
   {
      if ( (bRotary2Select || bRotary2RotatedCW || bRotary2RotatedCCW) && (!popups_has_popup(g_pPopupCameraParams)) )
      {
         if ( NULL == g_pPopupCameraParams )
            g_pPopupCameraParams = new PopupCameraParams();
         popups_add_topmost(g_pPopupCameraParams);
         g_pPopupCameraParams->invalidate();
         bRotary2Select = false;
      }
      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
         g_pPopupCameraParams->handleRotaryEvents(bRotary2RotatedCW, bRotary2RotatedCCW, bRotary2RotatedFastCW, bRotary2RotatedFastCCW, bRotary2Select, bRotary2Cancel);
   }

   if ( (NULL == pCS) || (pCS->nRotaryEncoderFunction != 1) )
   {
      bRotarySelect = false;
      bRotaryCancel = false;
      bRotaryRotatedCW = false;
      bRotaryRotatedCCW = false;
      bRotaryRotatedFastCW = false;
      bRotaryRotatedFastCCW = false;
      bRotaryRotated = false;
      bRotaryEvents = false;
   }
   else
   {
      hardware_override_keys(bRotarySelect?1:0, bRotaryCancel?1:0, bRotaryRotatedCCW?1:0, bRotaryRotatedCW?1:0, 0, 0,0,0);
      bRotaryEvents = bRotarySelect | bRotaryCancel | bRotaryRotatedCW | bRotaryRotatedCCW | bRotaryRotatedFastCW | bRotaryRotatedFastCCW | bRotaryRotated;
   }

   if ( (NULL == pCS) || (pCS->nRotaryEncoderFunction2 != 1) )
   {
      bRotary2Select = false;
      bRotary2Cancel = false;
      bRotary2RotatedCW = false;
      bRotary2RotatedCCW = false;
      bRotary2RotatedFastCW = false;
      bRotary2RotatedFastCCW = false;
      bRotary2Rotated = false;
      bRotary2Events = false;
   }
   else
   {
      hardware_override_keys(bRotary2Select?1:0, bRotary2Cancel?1:0, bRotary2RotatedCCW?1:0, bRotary2RotatedCW?1:0, 0, 0,0,0);
      bRotary2Events = bRotary2Select | bRotary2Cancel | bRotary2RotatedCW | bRotary2RotatedCCW | bRotary2RotatedFastCW | bRotary2RotatedFastCCW | bRotary2Rotated;
   }
   if ( (NULL != pCS) && (pCS->nRotaryEncoderFunction == 1) && (pCS->nRotaryEncoderFunction2 == 1) )
      hardware_override_keys((bRotarySelect || bRotary2Select)?1:0, (bRotaryCancel || bRotary2Cancel)?1:0, (bRotaryRotatedCCW || bRotary2RotatedCCW)?1:0, (bRotaryRotatedCW || bRotary2RotatedCW)?1:0, 0, 0,0,0);

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MENU )
   {
      if ( osd_is_stats_flight_end_on() )
      {
         osd_remove_stats_flight_end();
         return;
      }
      //system("omxplayer -p -o hdmi res/sound_menu_click2.wav");
      if ( 0 == g_iMenuStackTopIndex )
      {
          load_Preferences();
          add_menu_to_stack(new MenuRoot());
      }
      else 
         gpMenuStack[g_iMenuStackTopIndex-1]->onSelectItem();
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_BACK )
   {
      if ( osd_is_stats_flight_end_on() )
      {
         osd_remove_stats_flight_end();
         return;
      }

      if ( (g_iMenuStackTopIndex == 0) && g_bHasVideoDecodeStatsSnapshot )
      {
         g_bHasVideoDecodeStatsSnapshot = false;
         return;
      }
      
      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
         g_pPopupCameraParams->handleRotaryEvents(false, false, false, false, false, true);
      else
      {
         //system("omxplayer -p -o hdmi res/sound_menu_click2.wav");
         int processed = 0;
         if ( g_iMenuStackTopIndex > 0 )
         if ( NULL != gpMenuStack[g_iMenuStackTopIndex-1] )
             processed = gpMenuStack[g_iMenuStackTopIndex-1]->onBack();
         if ( ! processed )
            menu_stack_pop();
      }
   }

   if ( isKeyBackLongPressed() )
   {
      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
         g_pPopupCameraParams->handleRotaryEvents(false, false, false, false, false, true);
      else
      {
         for( int i=0; i<g_iMenuStackTopIndex; i++ )
            if ( NULL != gpMenuStack[i] )
               gpMenuStack[i]->onBack();

         menu_close_all();
      }
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_PLUS )
   {
      if ( g_iMenuStackTopIndex > 0 )
      if ( NULL != gpMenuStack[g_iMenuStackTopIndex-1] )
         gpMenuStack[g_iMenuStackTopIndex-1]->onMoveDown(bRotaryRotated);
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MINUS )
   {
      if ( g_iMenuStackTopIndex > 0 )
      if ( NULL != gpMenuStack[g_iMenuStackTopIndex-1] )
         gpMenuStack[g_iMenuStackTopIndex-1]->onMoveUp(bRotaryRotated);
   }
}


void menu_refresh_all_menus()
{
   log_line("Menus: refresh all menus...");
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != gpMenuStack[i] )
         gpMenuStack[i]->onShow();
   }
   log_line("Menus: Refreshed all menus.");
}


void menu_render()
{
   float fOrigAlpha = g_pRenderEngine->getGlobalAlfa();
   Preferences* pP = get_Preferences();
   
   bool hasConfirmationDialogOnTop = false;
   if ( g_iMenuStackTopIndex > 1 )
   if ( (gpMenuStack[g_iMenuStackTopIndex-1]->m_MenuId == MENU_ID_CONFIRMATION) )
      hasConfirmationDialogOnTop = true;
   
   // If menus are stacked, render only last 3 menus

   int iMenuToRender = g_iMenuStackTopIndex-3;
   if ( iMenuToRender < 0 )
      iMenuToRender = 0;

   if ( ! pP->iMenusStacked )
      iMenuToRender = 0;

   while ( iMenuToRender < g_iMenuStackTopIndex )
   {
      g_pRenderEngine->setGlobalAlfa(s_fMenuGlobalAlpha);

      if ( gpMenuStack[iMenuToRender]->m_bDisableBackgroundAlpha )
      {
         while ( iMenuToRender < g_iMenuStackTopIndex )
         {
            gpMenuStack[iMenuToRender]->Render();
            iMenuToRender++;
         }
         break;
      }

      bool bNoBackgroundAdjustment = false;
      if ( hasConfirmationDialogOnTop )
      if ( (iMenuToRender == g_iMenuStackTopIndex-1) || (iMenuToRender == g_iMenuStackTopIndex-2) )
         bNoBackgroundAdjustment = true;

      if ( bNoBackgroundAdjustment )
      {
         gpMenuStack[iMenuToRender]->Render();
         iMenuToRender++;
         continue;
      }
      
      float fBgAlphaForMenu = gpMenuStack[iMenuToRender]->m_fAlfaWhenInBackground - 0.08 * (float)(g_iMenuStackTopIndex-iMenuToRender-1);
      if ( fBgAlphaForMenu < 0.0 )
         fBgAlphaForMenu = 0.0;

      u32 tMenuShowTime = gpMenuStack[iMenuToRender]->getOnShowTime();
      u32 tMenuChildAddTime = gpMenuStack[iMenuToRender]->getOnChildAddTime();
      u32 tMenuChildCloseTime = gpMenuStack[iMenuToRender]->getOnReturnFromChildTime();

      // On Show

      if ( g_TimeNow < tMenuShowTime + 250 )
      {
         float f = (g_TimeNow-tMenuShowTime)/250.0;
         if ( f < 0.0 ) f = 0.0;
         if ( f > 1.0 ) f = 1.0;
         float fTargetAlpha = s_fMenuGlobalAlpha * f + (1.0 - f) * fBgAlphaForMenu;
         g_pRenderEngine->setGlobalAlfa(fTargetAlpha);
      }

      // On child close

      if ( (0 != tMenuChildCloseTime) && (g_TimeNow < tMenuChildCloseTime+250) )
      {
         float f = (g_TimeNow-tMenuChildCloseTime)/250.0;
         if ( f < 0.0 ) f = 0.0;
         if ( f > 1.0 ) f = 1.0;
         float fTargetAlpha = s_fMenuGlobalAlpha * f + (1.0 - f) * fBgAlphaForMenu;
         g_pRenderEngine->setGlobalAlfa(fTargetAlpha);
      }

      // Child visible and not closing
      if ( (tMenuChildAddTime != 0) && (tMenuChildCloseTime == 0) )
      {
         if ( g_TimeNow >= tMenuChildAddTime + 250 )
            g_pRenderEngine->setGlobalAlfa(fBgAlphaForMenu);
         else
         {
            float f = (g_TimeNow-tMenuChildAddTime)/250.0;
            if ( f < 0.0 ) f = 0.0;
            if ( f > 1.0 ) f = 1.0;
            float fTargetAlpha = s_fMenuGlobalAlpha * (1.0 - f) + f * fBgAlphaForMenu;
            g_pRenderEngine->setGlobalAlfa(fTargetAlpha);
         }
      }
      gpMenuStack[iMenuToRender]->Render();
      iMenuToRender++;
   }
   g_pRenderEngine->setGlobalAlfa(fOrigAlpha);
}

bool menu_is_menu_on_top(Menu* pMenu)
{
   if ( g_iMenuStackTopIndex == 0 )
      return false;
   if ( gpMenuStack[g_iMenuStackTopIndex-1] != pMenu )
      return false;
   return true;
}

bool isMenuOn()
{
   if ( g_iMenuStackTopIndex == 0 )
      return false;
   return true;
}

void menu_invalidate_all()
{
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != gpMenuStack[i] )
         gpMenuStack[i]->invalidate();
   }
}

// It is called just before the new child menu is added to the stack.
// So it's not yet actually present in the stack.

void menu_startAnimationOnChildMenuAdd(Menu* pTopMenu)
{
   bool bDoAnimate = true;
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( (NULL != gpMenuStack[i]) && gpMenuStack[i]->m_bDisableStacking )
      {
         bDoAnimate = false;
         break;
      }
   }
   if ( ! bDoAnimate )
      return;
   
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL == gpMenuStack[i] )
         continue;

      gpMenuStack[i]->startAnimationOnChildMenuAdd();
      if ( gpMenuStack[i] == pTopMenu )
         break;
   }
}

void menu_startAnimationOnChildMenuClosed(Menu* pTopMenu)
{
   bool bDoAnimate = true;
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( (NULL != gpMenuStack[i]) && gpMenuStack[i]->m_bDisableStacking )
      {
         bDoAnimate = false;
         break;
      }
   }
   if ( ! bDoAnimate )
      return;
   
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL == gpMenuStack[i] )
         continue;

      gpMenuStack[i]->startAnimationOnChildMenuClosed();
      if ( gpMenuStack[i] == pTopMenu )
         break;
   }
}


bool menu_check_current_model_ok_for_edit()
{
   if ( NULL == g_pCurrentModel )
   {
      Menu* pm = new Menu(0,"Info",NULL);
      pm->m_xPos = 0.24; pm->m_yPos = 0.2;
      pm->m_Width = 0.3;
      pm->addTopLine("You have no vehicles. Search and connect to a vehicle first.");
      add_menu_to_stack(pm);
      return false;
   }
      
   if ( g_pCurrentModel->is_spectator )
   {
         Menu* pm = new Menu(0,"Info",NULL);
         pm->m_xPos = 0.24; pm->m_yPos = 0.2;
         pm->m_Width = 0.3;
         pm->addTopLine("You are currently paired with a vehicle in spectator mode. You can not change any vehicle settings while in spectator mode.");
         add_menu_to_stack(pm);
         return false;
   }

   return true;
}

bool menu_has_animation_in_progress()
{
   if ( g_iMenuStackTopIndex < 2 )
      return false;
   return true;
}
