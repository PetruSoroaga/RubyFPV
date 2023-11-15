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
#include "../base/models.h"
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "../base/ctrl_interfaces.h"
#include "../base/utils.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_vehicle_rc_channels.h"
#include "menu_confirmation.h"
#include <math.h>

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "osd_common.h"
#include "timers.h"

MenuVehicleRCChannels::MenuVehicleRCChannels(void)
:Menu(MENU_ID_VEHICLE_RC_CHANNELS, "Channels Assignment", NULL)
{
   m_Width = 0.66;
   m_xPos = menu_get_XStartPos(m_Width)-0.01; m_yPos = 0.24;
   setColumnsCount(8);
   float fSliderWidth = 0.14;
   char szBuff[128];

   m_ExtraHeight = 1.9*menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE;
   
   m_iCurrentChannelToAssign = -1;
   m_bWaitingForInput = false;
   m_pPopupAssignment = NULL;

   m_ChannelCount = g_pCurrentModel->rc_params.channelsCount; 
   for( int i=0; i<m_ChannelCount; i++ )
   {
      m_CurrentRCValues[i] = 0;

      g_pCurrentModel->get_rc_channel_name(i, szBuff);

      m_ItemsChannels[i].pItemTitle = new MenuItem(szBuff);
      m_ItemsChannels[i].m_IndexTitle = addMenuItem(m_ItemsChannels[i].pItemTitle);

      m_ItemsChannels[i].pItemMin = new MenuItemRange("Min:", "", 500, 2000, 1000, 1 );  
      m_ItemsChannels[i].pItemMin->setCondensedOnly();
      m_ItemsChannels[i].m_IndexMin = addMenuItem(m_ItemsChannels[i].pItemMin);

      m_ItemsChannels[i].pItemMid = new MenuItemRange("Mid:", "", 1000, 2000, 1500, 1 );  
      m_ItemsChannels[i].pItemMid->setCondensedOnly();
      m_ItemsChannels[i].m_IndexMid = addMenuItem(m_ItemsChannels[i].pItemMid);

      m_ItemsChannels[i].pItemMax = new MenuItemRange("Max:", "", 1500, 2500, 2000, 1 );  
      m_ItemsChannels[i].pItemMax->setCondensedOnly();
      m_ItemsChannels[i].m_IndexMax = addMenuItem(m_ItemsChannels[i].pItemMax);

      m_ItemsChannels[i].pItemReverse = new MenuItemCheckbox("Reverse", "Reverse this channel output");
      m_ItemsChannels[i].pItemReverse->setCondensedOnly();
      m_ItemsChannels[i].m_IndexReverse = addMenuItem(m_ItemsChannels[i].pItemReverse);
      m_ItemsChannels[i].pItemReverse->setNotEditable();
      
      m_ItemsChannels[i].pItemToggle = new MenuItemCheckbox("Toggle", "Make this channel togle when buttons are pressed");
      m_ItemsChannels[i].pItemToggle->setCondensedOnly();
      m_ItemsChannels[i].m_IndexToggle = addMenuItem(m_ItemsChannels[i].pItemToggle);
      m_ItemsChannels[i].pItemToggle->setNotEditable();
      
      m_ItemsChannels[i].pItemClear = new MenuItem("Clear", "Clears the assignment for this channel");
      m_ItemsChannels[i].pItemClear->setCondensedOnly();
      m_ItemsChannels[i].m_IndexClear = addMenuItem(m_ItemsChannels[i].pItemClear);

      m_ItemsChannels[i].pItemAssign = new MenuItem("Assign", "Assign a HID axe/button/joystick/pot to this channel. Or assign multiple buttons to this channel.");
      m_ItemsChannels[i].pItemAssign->setCondensedOnly();
      //m_ItemsChannels[i].pItemAssign->showArrow();
      m_ItemsChannels[i].m_IndexAssign = addMenuItem(m_ItemsChannels[i].pItemAssign);
   }

   m_JoystickIndex = -1;
   m_pJoystick = NULL;
   log_line("Trying to find the primary HID interface for this vehicle...");
   controllerInterfacesEnumJoysticks();

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   for( int i=0; i<pCI->inputInterfacesCount; i++ )
   {
      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
      if ( NULL == pCII )
         continue;
      if ( pCII->uId == g_pCurrentModel->rc_params.hid_id )
      {
         m_JoystickIndex = i;
         m_pJoystick = controllerInterfacesGetAt(m_JoystickIndex);
         hardware_open_joystick( m_pJoystick->currentHardwareIndex );
         log_line("Found the HID interface, id: %u", pCII->uId);
         break;
      }
   }

   m_TimeLastJoystickCheck = get_current_timestamp_ms();
   m_TimeLastRCCompute = get_current_timestamp_ms();
   m_TimeLastRCCompute = g_TimeNow;
}

MenuVehicleRCChannels::~MenuVehicleRCChannels()
{
   if ( NULL != m_pJoystick )
      hardware_close_joystick( m_pJoystick->currentHardwareIndex );

   if ( NULL != m_pPopupAssignment )
   {
      popups_remove(m_pPopupAssignment);
      delete m_pPopupAssignment;
      m_pPopupAssignment = NULL;
   }
}

void MenuVehicleRCChannels::onShow()
{
   Menu::onShow();
   m_TimeLastRCCompute = g_TimeNow;

   if ( -1 == m_JoystickIndex )
   {
      addMessage("The assigned input joystick is not present. Assign the current joystick to this vehicle first!");
      log_line("HID interface for this vehicle not found.");
      return;
   }

   if ( NULL == m_pJoystick || ( ! m_pJoystick->bCalibrated ) )
   {
      addMessage("This joystick is not calibrated! Calibrate it first!");
      return;
   }
}

void MenuVehicleRCChannels::valuesToUI()
{
   for( int i=0; i<m_ChannelCount; i++ )
   {
      m_ItemsChannels[i].pItemMin->setCurrentValue(g_pCurrentModel->rc_params.rcChMin[i]);
      m_ItemsChannels[i].pItemMid->setCurrentValue(g_pCurrentModel->rc_params.rcChMid[i]);
      m_ItemsChannels[i].pItemMax->setCurrentValue(g_pCurrentModel->rc_params.rcChMax[i]);
      m_ItemsChannels[i].pItemAssign->setEnabled(true);

      if ( g_pCurrentModel->rc_params.rcChFlags[i] & RC_CH_FLAGS_INVERTED )
         m_ItemsChannels[i].pItemReverse->setChecked(true);
      else
         m_ItemsChannels[i].pItemReverse->setChecked(false);

      if ( g_pCurrentModel->rc_params.rcChAssignment[i] & RC_CH_ASSIGNMENT_FLAG_TOGGLE )
         m_ItemsChannels[i].pItemToggle->setChecked(true);
      else
         m_ItemsChannels[i].pItemToggle->setChecked(false);

      if ( g_pCurrentModel->rc_params.rcChAssignment[i] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED )
         m_ItemsChannels[i].pItemClear->setEnabled(true);
      else
         m_ItemsChannels[i].pItemClear->setEnabled(false);
   }
}

void MenuVehicleRCChannels::Render()
{
   RenderPrepare();
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();
   float hTop = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();

   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   g_pRenderEngine->setColors(get_Color_MenuText());

   float xSt = m_RenderXPos+MENU_MARGINS*menu_getScaleMenus();
   g_pRenderEngine->drawText(xSt + 0.094*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Min:");
   g_pRenderEngine->drawText(xSt + 0.15*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Mid:");
   g_pRenderEngine->drawText(xSt + 0.20*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Max:");
   g_pRenderEngine->drawText(xSt + 0.255*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Inv:");
   g_pRenderEngine->drawText(xSt + 0.295*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Toggle:");
   g_pRenderEngine->drawText(xSt + 0.43*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Assignment:");
   g_pRenderEngine->drawText(xSt + 0.55*menu_getScaleMenus(), y, height_text, g_idFontMenu, "Live View:");
    
   y += hTop*1.9;

   float yMenuItemHeight = m_pMenuItems[0]->getItemHeight(getUsableWidth());
   yMenuItemHeight += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_ITEM_SPACING;

   for( int i=0; i<m_ChannelCount; i++ )
   {
      float yLine = y+i*yMenuItemHeight-hTop*0.3;
      char szBuff[128];
      szBuff[0] = 0;
      strcpy(szBuff, "None");
      if ( g_pCurrentModel->rc_params.rcChAssignment[i] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED )
      {
         if ( g_pCurrentModel->rc_params.rcChAssignment[i] & RC_CH_ASSIGNMENT_FLAG_BUTTON )
         {
            szBuff[0] = 0;
            for( int shift = 4; shift <= 24; shift += 4 )
            {
            if ( ((g_pCurrentModel->rc_params.rcChAssignment[i] >> shift) & 0x0F) > 0 )
            if ( strlen(szBuff)<20 )
            {
               char szTmp[32];
               if ( 0 == szBuff[0] )
                  snprintf(szBuff, sizeof(szBuff), "B-%d", ((g_pCurrentModel->rc_params.rcChAssignment[i] >> shift) & 0x0F));
               else
               {
                  snprintf(szTmp, sizeof(szTmp), ", B-%d", ((g_pCurrentModel->rc_params.rcChAssignment[i] >> shift) & 0x0F));
                  strlcat(szBuff, szTmp, sizeof(szBuff));
               }
            }
            }
         }
         else if ( ((g_pCurrentModel->rc_params.rcChAssignment[i] >> 4 ) & 0x0F) > 0 )
            snprintf(szBuff, sizeof(szBuff), "A-%d", (((g_pCurrentModel->rc_params.rcChAssignment[i]) >>4 ) & 0x0F));
      }

      if ( strlen(szBuff) > 5 )
         g_pRenderEngine->drawMessageLines(xSt + 0.48*menu_getScaleMenus(), yLine, szBuff, height_text*0.76, MENU_TEXTLINE_SPACING, 0.08*menu_getScaleMenus(), g_idFontMenu);
      else
         g_pRenderEngine->drawText(xSt + 0.48*menu_getScaleMenus(), yLine, height_text, g_idFontMenu, szBuff);

      int val = m_CurrentRCValues[i];
      float xLive = xSt + 0.55*menu_getScaleMenus();
      float corner = 0.003*menu_getScaleMenus();
      float padding = 0.002*menu_getScaleMenus()+g_pRenderEngine->getPixelWidth();
      float rectW = 0.06*menu_getScaleMenus();
      float rectH = yMenuItemHeight * 0.6;
      rectW += 2*padding;
      rectH += 2*padding;
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,100,0,0.8);
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->setStrokeSize(0);
      if ( 0 != val )
         g_pRenderEngine->drawRoundRect(xLive+padding*0.5, yLine+g_pRenderEngine->getPixelHeight(), (rectW-padding)*(float)(val-g_pCurrentModel->rc_params.rcChMin[i])/(g_pCurrentModel->rc_params.rcChMax[i]-g_pCurrentModel->rc_params.rcChMin[i]), rectH-2*g_pRenderEngine->getPixelHeight(), corner);

      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xLive, yLine, rectW, rectH, corner);
   
      g_pRenderEngine->setColors(get_Color_MenuText());
      snprintf(szBuff, sizeof(szBuff), "%d", val);
      if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
      if ( ! (g_pCurrentModel->rc_params.rcChAssignment[i] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED ) )
         snprintf(szBuff, sizeof(szBuff), "Not Assigned");

      g_pRenderEngine->drawText(xLive+0.003*menu_getScaleMenus(), yLine, height_text, g_idFontMenu, szBuff);
      g_pRenderEngine->setColors(get_Color_MenuText());
   }

   float fSelWidth = 0.05*menu_getScaleMenus();
   for( int i=0; i<m_ItemsCount; i++ )
   {
      m_pMenuItems[i]->invalidate();
      m_pMenuItems[i]->getItemHeight(getSelectionWidth());
      m_pMenuItems[i]->getTitleWidth(getSelectionWidth());
      m_pMenuItems[i]->getValueWidth(getSelectionWidth());
      int subIndex = i % m_iColumnsCount;
      if ( subIndex == 0 )
         m_pMenuItems[i]->Render(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus(), y, i == m_SelectedIndex, fSelWidth*1.2);
      else
      {

         float dx = fSelWidth*1.0;// + 0.04*menu_getScaleMenus();
         if ( subIndex < 4 )
            dx = fSelWidth*0.3 + 0.02*menu_getScaleMenus() + fSelWidth * 1.1 * (i % m_iColumnsCount);
         else
            dx = fSelWidth*1.02 + 0.01*menu_getScaleMenus() + fSelWidth * 1.0 * (i % m_iColumnsCount);

         if ( subIndex == 6 )
         {
            dx = 0.36*menu_getScaleMenus();// + 0.04*menu_getScaleMenus();
            m_pMenuItems[i]->Render(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()+dx, y, i == m_SelectedIndex, 0.048*menu_getScaleMenus());
         }
         else if ( subIndex == 7 )
         {
            dx = 0.42*menu_getScaleMenus();// + 0.04*menu_getScaleMenus();
            m_pMenuItems[i]->Render(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()+dx, y, i == m_SelectedIndex, 0.056*menu_getScaleMenus());
         }
         else
            m_pMenuItems[i]->RenderCondensed(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus() + dx, y, i == m_SelectedIndex, 0);
      }
      if ( subIndex == m_iColumnsCount-1 )
      {
         y += m_pMenuItems[i]->getItemHeight(getUsableWidth());
         y += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_ITEM_SPACING;
      }
   }
   RenderEnd(yTop);
}

int MenuVehicleRCChannels::onBack()
{
   if ( m_bWaitingForInput )
   {
      m_iCurrentChannelToAssign = -1;
      m_bWaitingForInput = false;

      if ( NULL != m_pPopupAssignment )
      {
         popups_remove(m_pPopupAssignment);
         delete m_pPopupAssignment;
         m_pPopupAssignment = NULL;
      }

      m_pPopupAssignment = new Popup(true, "Channel assignment canceled", 3 );
      m_pPopupAssignment->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(m_pPopupAssignment);

      return 1;
   }

   return Menu::onBack();
}


bool MenuVehicleRCChannels::periodicLoop()
{
   if ( NULL == m_pJoystick )
      return false;
   if ( m_bAnimating )
      return false;
   u32 timeNow = get_current_timestamp_ms();
   u32 miliSec = timeNow - m_TimeLastRCCompute;
   m_TimeLastRCCompute = timeNow;

   hw_joystick_info_t* pJoy = hardware_get_joystick_info(m_pJoystick->currentHardwareIndex);
   if ( NULL == pJoy )
   {
      for( int i=0; i<m_ChannelCount; i++ )
         m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], g_pSM_RCIn, NULL, m_pJoystick, miliSec);
      return false;
   }

   if ( 0 == hardware_is_joystick_opened(m_pJoystick->currentHardwareIndex) )
   {
      for( int i=0; i<m_ChannelCount; i++ )
         m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], g_pSM_RCIn, NULL, m_pJoystick, miliSec);

      if ( get_current_timestamp_ms() > m_TimeLastJoystickCheck + 500 )
      {
         m_TimeLastJoystickCheck = get_current_timestamp_ms();
         hardware_open_joystick( m_pJoystick->currentHardwareIndex );
      }
      return false;
   }

   int res = hardware_read_joystick( m_pJoystick->currentHardwareIndex, 10 );

   if ( res < 0 )
   {
      hardware_close_joystick(m_pJoystick->currentHardwareIndex);
      return false;
   }
   for( int i=0; i<m_ChannelCount; i++ )
      m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], g_pSM_RCIn, pJoy, m_pJoystick, miliSec);


   if ( m_bWaitingForInput && (-1 != m_iCurrentChannelToAssign) )
   {
      for( int i=0; i<pJoy->countButtons; i++ )
      {
         //if ( pJoy->buttonsValues[i] != m_pJoystick->buttonsReleasedValue[i] )
         if ( m_ButtonsBeforeAssign[i] != pJoy->buttonsValues[i] )
         {
            log_line("Assign button %d, read value: %d", i+1, pJoy->buttonsValues[i]);
            onAssignButton(i);
            return true;
         }
      }
      for( int i=0; i<pJoy->countAxes; i++ )
      {
         float fMax = fabs(m_pJoystick->axesMaxValue[i] - m_pJoystick->axesMinValue[i]);
         if ( fMax < 20 )
            continue;
         if ( fabs(pJoy->axesValues[i] - m_AxesBeforeAssign[i]) < fMax*0.3 )
            continue;
         if ( pJoy->axesValues[i] < m_pJoystick->axesMinValue[i] ||
              pJoy->axesValues[i] > m_pJoystick->axesMaxValue[i] )
            continue;

         log_line("Assign axe %d, read value: %d, initial value: %d, min/max: %d/%d", i+1, pJoy->axesValues[i], m_AxesBeforeAssign[i], m_pJoystick->axesMinValue[i], m_pJoystick->axesMaxValue[i]);
         onAssignAxe(i);
         return true;

         if ( m_pJoystick->axesMinValue[i] < m_pJoystick->axesCenterValue[i]-20 &&
              pJoy->axesValues[i] <= (m_pJoystick->axesMinValue[i]+m_pJoystick->axesCenterValue[i])*0.6 )
         {
            log_line("Assign axe %d, read value: %d, initial value: %d, min/max: %d/%d", i+1, pJoy->axesValues[i], m_AxesBeforeAssign[i], m_pJoystick->axesMinValue[i], m_pJoystick->axesMaxValue[i]);
            onAssignAxe(i);
            return true;
         }

         if ( m_pJoystick->axesMaxValue[i] > m_pJoystick->axesCenterValue[i]+20 &&
              pJoy->axesValues[i] >= (m_pJoystick->axesMaxValue[i]+m_pJoystick->axesCenterValue[i])*0.6 )
         {
            log_line("Assign axe %d, read value: %d, initial value: %d, min/max: %d/%d", i+1, pJoy->axesValues[i], m_AxesBeforeAssign[i], m_pJoystick->axesMinValue[i], m_pJoystick->axesMaxValue[i]);
            onAssignAxe(i);
            return true;
         }
      }
      return true;
   }

   return true;
}


void MenuVehicleRCChannels::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicleRCChannels::onAssignButton(int buttonIndex)
{
   if ( (!m_bWaitingForInput) || (-1 == m_iCurrentChannelToAssign) )
      return;

   if ( NULL != m_pPopupAssignment )
   {
      popups_remove(m_pPopupAssignment);
      delete m_pPopupAssignment;
      m_pPopupAssignment = NULL;
   }

   char szBuff[64];
   snprintf(szBuff, sizeof(szBuff), "Assigned button %d to channel %d", buttonIndex+1, m_iCurrentChannelToAssign+1);
   m_pPopupAssignment = new Popup(true, szBuff, 3 );
   m_pPopupAssignment->setIconId(g_idIconInfo, get_Color_IconNormal());
   popups_add_topmost(m_pPopupAssignment);

   rc_parameters_t params;
   populateRCInfo(&params);

   u32 button = buttonIndex+1;
   if ( button > 15 )
      button = 15;

   // No assignment?
   if ( ! (params.rcChAssignment[m_iCurrentChannelToAssign] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) )
   {
      params.rcChAssignment[m_iCurrentChannelToAssign] = RC_CH_ASSIGNMENT_FLAG_ASSIGNED | RC_CH_ASSIGNMENT_FLAG_BUTTON;
      params.rcChAssignment[m_iCurrentChannelToAssign] |= (button << 4);
   }
   // Was assigned to an axe?
   else if ( (params.rcChAssignment[m_iCurrentChannelToAssign] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) &&
             (!(params.rcChAssignment[m_iCurrentChannelToAssign] & RC_CH_ASSIGNMENT_FLAG_BUTTON)))
   {
      params.rcChAssignment[m_iCurrentChannelToAssign] = RC_CH_ASSIGNMENT_FLAG_ASSIGNED | RC_CH_ASSIGNMENT_FLAG_BUTTON;
      params.rcChAssignment[m_iCurrentChannelToAssign] |= (button << 4);
   }
   // Was assigned to a button
   else
   {
      log_line("Assign multibutton: Add button: %d", button);
      params.rcChAssignment[m_iCurrentChannelToAssign] |= RC_CH_ASSIGNMENT_FLAG_MULTIPLE;
      for( int shift = 4; shift<=24; shift += 4 )
      {
         if ( ((params.rcChAssignment[m_iCurrentChannelToAssign] >> shift) & 0x0F) == 0 )
         {
            u32 addBits = (button << shift);
            params.rcChAssignment[m_iCurrentChannelToAssign] |= addBits;
            log_line("Assign multibutton: Added button: %d to position %d shift", button, shift);
            break;
         }
      }
   }

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();

   m_bWaitingForInput = false;
   m_iCurrentChannelToAssign = -1;
}

void MenuVehicleRCChannels::onAssignAxe(int axeIndex)
{
   if ( (!m_bWaitingForInput) || (-1 == m_iCurrentChannelToAssign) )
      return;

   if ( NULL != m_pPopupAssignment )
   {
      popups_remove(m_pPopupAssignment);
      delete m_pPopupAssignment;
      m_pPopupAssignment = NULL;
   }

   char szBuff[64];
   snprintf(szBuff, sizeof(szBuff), "Assigned axe %d to channel %d", axeIndex+1, m_iCurrentChannelToAssign+1);
   m_pPopupAssignment = new Popup(true, szBuff, 3 );
   m_pPopupAssignment->setIconId(g_idIconInfo, get_Color_IconNormal());
   popups_add_topmost(m_pPopupAssignment);

   rc_parameters_t params;
   populateRCInfo(&params);

   u32 axe = axeIndex+1;
   if ( axe > 15 )
      axe = 15;
   params.rcChAssignment[m_iCurrentChannelToAssign] = RC_CH_ASSIGNMENT_FLAG_ASSIGNED;
   params.rcChAssignment[m_iCurrentChannelToAssign] |= (axe << 4);

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();

   m_bWaitingForInput = false;
   m_iCurrentChannelToAssign = -1;
}

void MenuVehicleRCChannels::onClickAssign(int nChannel)
{
   if ( m_bWaitingForInput )
      return;

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( NULL == m_pJoystick )
   {
      m_JoystickIndex = -1;
      log_line("Trying to find the primary HID interface for this vehicle...");
      controllerInterfacesEnumJoysticks();

      for( int i=0; i<pCI->inputInterfacesCount; i++ )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
         if ( NULL == pCII )
            continue;
         if ( pCII->uId == g_pCurrentModel->rc_params.hid_id )
         {
         m_JoystickIndex = i;
         m_pJoystick = controllerInterfacesGetAt(m_JoystickIndex);
         hardware_open_joystick( m_pJoystick->currentHardwareIndex );
         log_line("Found the HID interface, id: %u", pCII->uId);
         break;
         }
      }

      if ( -1 == m_JoystickIndex )
         log_line("HID interface for this vehicle not found.");
   }

   if ( 0 == pCI->inputInterfacesCount )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"No Input Devices",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("No input devices detected.");
      pm->addTopLine("A. Connect a joystick, gamepad or RC transmitter to a controller USB port.");
      pm->addTopLine("B. Check Controller->Peripherals menu to confirm that your input device is detected by the system.");
      pm->addTopLine("C. Calibrate the input device and you are done.");
      add_menu_to_stack(pm);
      return;
   }
   if ( -1 == m_JoystickIndex )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Primary Input Device Not Found",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("The primary input device for this vehicle was not found (not plugged in).");
      pm->addTopLine("Plug in the input device you assigned to this vehicle or go back to previous menu and change the primary input device for this vehicle.");
      add_menu_to_stack(pm);
      return;
   }

   t_ControllerInputInterface* pCII = controllerInterfacesGetAt(m_JoystickIndex);
   if ( ! pCII->bCalibrated )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Input Device Not Calibrated",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("The input device is not callibrated.");
      pm->addTopLine("Go to Controller->Pheriperals and calibrate your input device.");
      pm->addTopLine("You only have to calibrate your input device once.");
      add_menu_to_stack(pm);
      return;
   }

   hw_joystick_info_t* pJoy = hardware_get_joystick_info(m_pJoystick->currentHardwareIndex);
   if ( NULL == pJoy )
      return;

   for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
      m_AxesBeforeAssign[i] = pJoy->axesValues[i];

   for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
      m_ButtonsBeforeAssign[i] = pJoy->buttonsValues[i];

   m_iCurrentChannelToAssign = nChannel;
   m_bWaitingForInput = true;

   if ( NULL != m_pPopupAssignment )
   {
      popups_remove(m_pPopupAssignment);
      delete m_pPopupAssignment;
      m_pPopupAssignment = NULL;
   }

   m_pPopupAssignment = new Popup(true, "Assign to channel", 60 );
   m_pPopupAssignment->setIconId(g_idIconInfo, get_Color_IconNormal());
   m_pPopupAssignment->addLine("* Move the joystick/gimbal/pot axe that you want to assign to this RC channel;");
   m_pPopupAssignment->addLine("* Or press the button you want to assign/add to this RC channel;");
   m_pPopupAssignment->addLine("Press [Cancel/Back] key to cancel.");
   popups_add_topmost(m_pPopupAssignment);
}

void MenuVehicleRCChannels::populateRCInfo(rc_parameters_t* pRCInfo)
{
   if ( NULL == pRCInfo )
      return;
   memcpy(pRCInfo, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   for( int i=0; i<m_ChannelCount; i++ )
   {
      pRCInfo->rcChMin[i] = m_ItemsChannels[i].pItemMin->getCurrentValue();
      pRCInfo->rcChMid[i] = m_ItemsChannels[i].pItemMid->getCurrentValue();
      pRCInfo->rcChMax[i] = m_ItemsChannels[i].pItemMax->getCurrentValue();

      if ( m_ItemsChannels[i].pItemReverse->isChecked() )
         pRCInfo->rcChFlags[i] |= RC_CH_FLAGS_INVERTED;
      else
         pRCInfo->rcChFlags[i] &= (~RC_CH_FLAGS_INVERTED);

      if ( m_ItemsChannels[i].pItemToggle->isChecked() )
         pRCInfo->rcChAssignment[i] |= RC_CH_ASSIGNMENT_FLAG_TOGGLE;
      else
         pRCInfo->rcChAssignment[i] &= (~RC_CH_ASSIGNMENT_FLAG_TOGGLE);
   }
}

void MenuVehicleRCChannels::onSelectItem()
{
   int index = m_SelectedIndex/m_iColumnsCount;
   int subIndex = m_SelectedIndex % m_iColumnsCount;

   if ( subIndex == 0 )
   {
      Menu::onSelectItem();
      return;
   }

   Menu::onSelectItem();
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   log_line("selected subindex: %d", subIndex);

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;
   
   if ( subIndex == 7 )
   {
      onClickAssign(index);
      return;
   }

   rc_parameters_t params;
   populateRCInfo(&params);

   if ( subIndex == 6 )
   {
      params.rcChAssignment[index] = 0;
   }

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();  
}