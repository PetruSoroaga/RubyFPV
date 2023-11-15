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
#include "menu_controller_joystick.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"
#include <math.h>

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"

#include "../renderer/render_engine.h"
#include "render_joysticks.h"

#include "pairing.h"
#include "colors.h"
#include "osd_common.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"
#include "timers.h"

u32 s_uTimeNextJoystickInit = 0;

MenuControllerJoystick::MenuControllerJoystick(int joystickIndex)
:Menu(MENU_ID_CONTROLLER_JOYSTICK, "Input Device Settings", NULL)
{
   float text_scaleT = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();

   m_Width = 0.45;
   m_Height = 0.71;
   //m_xPos = menu_get_XStartPos(m_Width)-0.01;
   m_xPos = 0.14;
   m_yPos = 0.1;
   m_ExtraHeight = 1.7 * g_pRenderEngine->getMessageHeight("HUDID", text_scaleT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);

   m_JoystickIndex = joystickIndex;
   m_bIsCalibratingCenters = false;
   m_bIsCalibratingAxes = false;
   m_bCalibrationComplete = false;
   m_bCalibrationCanceled = false;
   m_topTextHeight = 0;
   m_pPopupCal = NULL;

   m_IndexBack = addMenuItem(new MenuItem("Back", "Close the input device window"));
   m_pItemsRange[0] = new MenuItemRange("Center Dead Zone", "Sets how big the center zone on sticks and joysticks should be.", 0, 10, 1, 0.1 );
   m_pItemsRange[0]->setSufix("%");
   m_IndexCenterBand = addMenuItem(m_pItemsRange[0]);

   m_IndexCalibrate = addMenuItem(new MenuItem("Calibrate", "Starts a calibration of the center position and travel ranges for all sticks, pots and joysticks."));

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   m_pJoystick = controllerInterfacesGetAt(m_JoystickIndex);
   hardware_open_joystick( m_pJoystick->currentHardwareIndex );

   if ( NULL != m_pJoystick )
   {
      char szBuff[256];
      snprintf(szBuff, sizeof(szBuff), "%s Settings", m_pJoystick->szInterfaceName );
      setTitle(szBuff);
   }

   if ( NULL != m_pJoystick )
   for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
   {
      axesCalMinValue[i] = m_pJoystick->axesMinValue[i];
      axesCalMaxValue[i] = m_pJoystick->axesMaxValue[i];
      axesCalCenterValue[i] = m_pJoystick->axesCenterValue[i];
   }
   for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
   {
      buttonsReleasedValue[i] = m_pJoystick->buttonsReleasedValue[i];
   }
}

MenuControllerJoystick::~MenuControllerJoystick()
{
   if ( NULL != m_pJoystick )
      hardware_close_joystick( m_pJoystick->currentHardwareIndex );

   if ( NULL != m_pPopupCal )
   {
      popups_remove(m_pPopupCal);
      delete m_pPopupCal;
      m_pPopupCal = NULL;
   }
}

void MenuControllerJoystick::valuesToUI()
{
   if ( NULL != m_pJoystick )
      m_pItemsRange[0]->setCurrentValue((float)m_pJoystick->axesCenterZone[0]/10.0);
}

void MenuControllerJoystick::onShow()
{
   Menu::onShow();
}

bool MenuControllerJoystick::periodicLoop()
{
   if ( m_bAnimating )
      return false;

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( NULL == m_pJoystick )
   {
      if ( g_TimeNow < s_uTimeNextJoystickInit )
         return true;

      controllerInterfacesEnumJoysticks();
      if ( 0 == pCI->inputInterfacesCount )
         return true;
      if ( pCI->inputInterfacesCount <= m_JoystickIndex )
         return true;

      m_pJoystick = controllerInterfacesGetAt(m_JoystickIndex);
      if ( NULL == m_pJoystick )
         return true;
      hardware_open_joystick( m_pJoystick->currentHardwareIndex );
      return true;
   }

   int res = hardware_read_joystick( m_pJoystick->currentHardwareIndex, 10 );
   if ( res < 0 )
   {
      m_pJoystick = NULL;
      hardware_sleep_ms(10);
      s_uTimeNextJoystickInit = g_TimeNow+1000;
      return true;
   }

   hw_joystick_info_t* pJoy = hardware_get_joystick_info(m_pJoystick->currentHardwareIndex);

   if ( m_bIsCalibratingAxes )
   {
      for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
      {
         if ( pJoy->axesValues[i] < axesCalMinValue[i] )
            axesCalMinValue[i] = pJoy->axesValues[i];
         if ( pJoy->axesValues[i] > axesCalMaxValue[i] )
            axesCalMaxValue[i] = pJoy->axesValues[i];

         if ( axesCalCenterValue[i] < axesCalMinValue[i] )
            axesCalCenterValue[i] = axesCalMinValue[i];
         if ( axesCalCenterValue[i] > axesCalMaxValue[i] )
            axesCalCenterValue[i] = axesCalMaxValue[i];
      }
   }

   if ( m_bIsCalibratingCenters )
   {
      for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
      {
         axesCalCenterValue[i] = pJoy->axesValues[i];

         if ( axesCalCenterValue[i] < axesCalMinValue[i] )
            axesCalCenterValue[i] = axesCalMinValue[i];
         if ( axesCalCenterValue[i] > axesCalMaxValue[i] )
            axesCalCenterValue[i] = axesCalMaxValue[i];
      }

      for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
         buttonsReleasedValue[i] = pJoy->buttonsValues[i];
   }

   return true;
}

void MenuControllerJoystick::Render()
{
   float height_textT = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();
   float height_text = MENU_FONT_SIZE_ITEMS*menu_getScaleMenus();

   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( NULL != m_pJoystick )
   {
      char szBuff[256];
      snprintf(szBuff, sizeof(szBuff), "HID ID: %u", m_pJoystick->uId);
      y += 1.8*g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), y, szBuff, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   }

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   y += height_text;

   m_topTextHeight = 0;
   if ( m_bCalibrationComplete )
       m_topTextHeight += 1.2*g_pRenderEngine->drawMessageLines("Calibration Complete. You can now use this input device.", m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus(), y+m_topTextHeight, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   else if ( m_bCalibrationCanceled )
   {
       m_topTextHeight += 1.2*g_pRenderEngine->drawMessageLines("Calibration canceled.", m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus(), y+m_topTextHeight, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
       if ( (NULL != m_pJoystick) && (! m_pJoystick->bCalibrated) )
          m_topTextHeight += 1.2*g_pRenderEngine->drawMessageLines("This input device is not calibrated! Calibrate it before you can use it.", m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus(), y+m_topTextHeight, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   }
   else if ( m_bIsCalibratingCenters )
       m_topTextHeight += 1.2*g_pRenderEngine->drawMessageLines("[Calibrating Centers]: Move all the sticks, pots and joysticks to the center rest position, release all the buttons or move them to the min position if they have multiple positions; then press the [Menu/Ok] key. Press [Cancel/Back] key to cancel the calibration.", m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus(), y+m_topTextHeight, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   else if ( m_bIsCalibratingAxes )
       m_topTextHeight += 1.2*g_pRenderEngine->drawMessageLines("[Calibrating Axes]: Move all the sticks, pots and joysticks as far as they can move on all directions, toggle all buttons in all positions if they have multiple positions;  then press the [Menu/Ok] key when done. Press [Cancel/Back] key to cancel the calibration.", m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus(), y+m_topTextHeight, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   else if ( (NULL != m_pJoystick) && (! m_pJoystick->bCalibrated) )
       m_topTextHeight += 1.2*g_pRenderEngine->drawMessageLines("This input device is not calibrated! Calibrate it before you can use it.", m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus(), y+m_topTextHeight, height_textT, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);

   y += (1.7+2.0)*height_text;
   
   if ( NULL == m_pJoystick )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      RenderEnd(yTop);
      return;
   }

   hw_joystick_info_t* pJoy = hardware_get_joystick_info(m_pJoystick->currentHardwareIndex);

   float xPos = m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float width = m_Width*menu_getScaleMenus()-0.12*menu_getScaleMenus();
   float height = (m_yPos + m_RenderHeight - y) - 0.09*menu_getScaleMenus();

   render_joystick(xPos, y, width, height, pJoy->countAxes, pJoy->countButtons, &(pJoy->axesValues[0]), &(pJoy->buttonsValues[0]), &(m_pJoystick->axesCenterZone[0]), &(axesCalMinValue[0]), &(axesCalMaxValue[0]), &(axesCalCenterValue[0]));

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( m_bIsCalibratingCenters || m_bIsCalibratingAxes )
   {
      xPos += width + 0.8*MENU_MARGINS*menu_getScaleMenus();
      height_text = 0.7*MENU_FONT_SIZE_ITEMS*menu_getScaleMenus();

      for( int i=0; i<12; i++ )
      {
      char szBuff[128];
      if ( i < MAX_JOYSTICK_AXES )
      {
         snprintf(szBuff, sizeof(szBuff), "Axe %02d: %d", i+1, pJoy->axesValues[i]);
         g_pRenderEngine->drawText(xPos, y, height_text, g_idFontMenu, szBuff);
      }
      //if ( i < MAX_JOYSTICK_BUTTONS )
      //{
      //   snprintf(szBuff, sizeof(szBuff), "Btn %02d: %d", i+1, pJoy->buttonsValues[i]);
      //   draw_message(szBuff, xPos+toScreenX(0.15)*menu_getScaleMenus(), y-height_text, text_scale, render_getFontMenu());
      //}
      y += height_text*1.1;
      }
   }
   RenderEnd(yTop);
}

int MenuControllerJoystick::onBack()
{
   if ( NULL != m_pPopupCal )
   {
      popups_remove(m_pPopupCal);
      delete m_pPopupCal;
      m_pPopupCal = NULL;
   }

   if ( m_bIsCalibratingCenters || m_bIsCalibratingAxes )
   {
      m_pMenuItems[0]->setEnabled(true);
      m_pMenuItems[1]->setEnabled(true);
      m_bIsCalibratingCenters = false;
      m_bIsCalibratingAxes = false;
      m_bCalibrationComplete = false;
      m_bCalibrationCanceled = true;
      return 1;
   }
   return 0;
}


void MenuControllerJoystick::onSelectItem()
{
   if ( NULL != m_pJoystick )
   if ( m_bIsCalibratingCenters || m_bIsCalibratingAxes )
   {
      if ( isKeyMenuPressed() )
      {
         if ( m_bIsCalibratingAxes )
         {
            m_bIsCalibratingCenters = true;
            m_bIsCalibratingAxes = false;
            for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
            {
               m_pJoystick->axesMinValue[i] = axesCalMinValue[i];
               m_pJoystick->axesMaxValue[i] = axesCalMaxValue[i];
            }
            save_ControllerInterfacesSettings();

            if ( NULL != m_pPopupCal )
            {
               popups_remove(m_pPopupCal);
               delete m_pPopupCal;
               m_pPopupCal = NULL;
            }

            m_pPopupCal = new Popup("Calibrate Centers/Rest", 0.62, 0.3, 0.34, 60 );
            m_pPopupCal->setFont(g_idFontMenuSmall);
            m_pPopupCal->setFixedWidth(0.32);
            m_pPopupCal->setIconId(g_idIconInfo, get_Color_IconNormal());
            m_pPopupCal->addLine("* Move all the sticks, pots and joysticks to the center rest position;");
            m_pPopupCal->addLine("* Release all the buttons or move them to the min position if they have multiple positions;");
            m_pPopupCal->addLine("Press [Menu/Ok] key when done.");
            m_pPopupCal->addLine("Press [Cancel/Back] key to cancel the calibration.");
            popups_add_topmost(m_pPopupCal);
            return;
         }

         if ( m_bIsCalibratingCenters )
         {
            m_pMenuItems[0]->setEnabled(true);
            m_pMenuItems[1]->setEnabled(true);
            m_bIsCalibratingCenters = false;
            m_bIsCalibratingAxes = false;
            m_bCalibrationCanceled = false;
            m_pJoystick->bCalibrated = true;

            for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
            {
               m_pJoystick->axesCenterValue[i] = axesCalCenterValue[i];
               if ( fabs(m_pJoystick->axesCenterValue[i] - (m_pJoystick->axesMaxValue[i]+m_pJoystick->axesMinValue[i])/2 ) > 0.15*fabs(m_pJoystick->axesMaxValue[i]-m_pJoystick->axesMinValue[i]) )
                  m_pJoystick->axesCenterValue[i] = (m_pJoystick->axesMaxValue[i]+m_pJoystick->axesMinValue[i])/2;
            }
            for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
            {
               m_pJoystick->buttonsReleasedValue[i] = buttonsReleasedValue[i];
            }

            save_ControllerInterfacesSettings();
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         
            if ( NULL != m_pPopupCal )
            {
               popups_remove(m_pPopupCal);
               delete m_pPopupCal;
               m_pPopupCal = NULL;
            }
            m_pPopupCal = new Popup(true, "Calibration Completed", 5 );
            m_pPopupCal->setIconId(g_idIconInfo, get_Color_IconNormal());
            popups_add_topmost(m_pPopupCal);
            return;
         }
      }
      if ( isKeyBackPressed() )
      {
         m_pMenuItems[0]->setEnabled(true);
         m_pMenuItems[1]->setEnabled(true);
         m_bIsCalibratingCenters = false;
         m_bIsCalibratingAxes = false;
         m_bCalibrationComplete = false;
         m_bCalibrationCanceled = true;
         if ( NULL != m_pPopupCal )
         {
            popups_remove(m_pPopupCal);
            delete m_pPopupCal;
            m_pPopupCal = NULL;
         }
         m_pPopupCal = new Popup(true, "Calibration Canceled", 5 );
         m_pPopupCal->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(m_pPopupCal);
      }
      return;
   }

   Menu::onSelectItem();
   
   if ( m_IndexBack == m_SelectedIndex )
      menu_stack_pop(0);   

   if ( m_IndexCalibrate == m_SelectedIndex )
   {
      m_pMenuItems[0]->setEnabled(false);
      m_pMenuItems[1]->setEnabled(false);
      m_bIsCalibratingAxes = true;
      m_bIsCalibratingCenters = false;
      m_bCalibrationComplete = false;
      m_bCalibrationCanceled = false;

      for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
      {
         axesCalMinValue[i] = 0;
         axesCalMaxValue[i] = 0;
         axesCalCenterValue[i] = 0;
      }
      for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
      {
         buttonsReleasedValue[i] = 0;
      }

      if ( NULL != m_pPopupCal )
      {
         popups_remove(m_pPopupCal);
         delete m_pPopupCal;
         m_pPopupCal = NULL;
      }
      m_pPopupCal = new Popup("Calibrate Range", 0.62, 0.3, 0.34, 60 );
      m_pPopupCal->setFont(g_idFontMenuSmall);
      m_pPopupCal->setFixedWidth(0.32);
      //m_pPopupCal->setXPos( toScreenX(m_xPos) + toScreenX(m_Width)*menu_getScaleMenus() + toScreenX(0.07)*menu_getScaleMenus());
      m_pPopupCal->setIconId(g_idIconInfo, get_Color_IconNormal());
      m_pPopupCal->addLine("* Move all the sticks, pots and joysticks as far as they can move on all directions;");
      m_pPopupCal->addLine("* Toggle all buttons in all positions if they have multiple positions;");
      m_pPopupCal->addLine("Press [Menu/Ok] key when done.");
      m_pPopupCal->addLine("Press [Cancel/Back] key to cancel the calibration.");
      popups_add_topmost(m_pPopupCal);
   }

   if ( NULL != m_pJoystick )
   if ( m_IndexCenterBand == m_SelectedIndex && ! m_pMenuItems[m_IndexCenterBand]->isEditing() )
   {
      for( int i=0; i<m_pJoystick->countAxes; i++ )
         m_pJoystick->axesCenterZone[i] = m_pItemsRange[0]->getCurrentValue()*10.0;
      save_ControllerInterfacesSettings();
      send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
   }
}
