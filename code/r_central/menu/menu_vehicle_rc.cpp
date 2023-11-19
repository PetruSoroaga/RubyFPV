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

#include "../../base/utils.h"
#include "../../base/shared_mem_i2c.h"
#include <math.h>
#include "menu.h"
#include "menu_vehicle_rc.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_vehicle_rc_channels.h"
#include "menu_vehicle_rc_failsafe.h"
#include "menu_vehicle_rc_expo.h"
#include "menu_vehicle_rc_camera.h"
#include "menu_vehicle_rc_input.h"
#include "../osd/osd_common.h"

const char* s_szRCInfo1 = "Note: Ruby is always the primary RC link.";
const char* s_szRCInfo2 = "* If you have a secondary backup regular RC link, it's recomended to set Ruby RC Failsafe to \"No Values\" and disable GCS failsafe in Mission Planner or QGround Control.";
const char* s_szRCInfo3 = "* If you don't have a secondary backup regular RC link, it's recomended to set Ruby RC Failsafe to \"No Values\" and enable GCS failsafe in Mission Planner or QGround Control.";


MenuVehicleRC::MenuVehicleRC(void)
:Menu(MENU_ID_VEHICLE_RC, "Remote Control Settings", NULL)
{
   m_Width = 0.31;
   m_xPos = 0.14;
   m_yPos = 0.19;
   float fSliderWidth = 0.09;

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( 0 == pCI->inputInterfacesCount )
      controllerInterfacesEnumJoysticks();

   m_IndexThrotleReverse = -1;
   m_bWaitingForInput = false;
   m_pPopupAssignment = NULL;

   m_pItemsSelect[0] = new MenuItemSelect("Remote Control", "Enables or disables the remote control functionality. If enabled, Ruby is the Primary RC link to the vehicle.");
   m_pItemsSelect[0]->addSelection("Disabled");
   m_pItemsSelect[0]->addSelection("Enabled");
   m_pItemsSelect[0]->setIsEditable();
   //m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexRCEnabled = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[10] = new MenuItemSelect("Enable RC Output", "Enables the final output of RC data to the flight controller. Turn this off while you are configuring the RC link. When everything is configured and looks right, turn this on to actually send the RC data to the flight controller.");  
   m_pItemsSelect[10]->addSelection("Disabled");
   m_pItemsSelect[10]->addSelection("Enabled");
   m_pItemsSelect[10]->addSelection("Use QA Button 1");
   m_pItemsSelect[10]->addSelection("Use QA Button 2");
   m_pItemsSelect[10]->addSelection("Use QA Button 3");
   m_pItemsSelect[10]->setIsEditable();
   //m_pItemsSelect[10]->setUseMultiViewLayout();
   m_IndexEnableOutput = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[16] = new MenuItemSelect("RC Input Source", "Sets the input source for the RC controlls.");  
   m_pItemsSelect[16]->addSelection("None");
   m_pItemsSelect[16]->addSelection("USB HID device");
   m_pItemsSelect[16]->addSelection("RC In (SBUS/IBUS)");
   m_pItemsSelect[16]->setEnabled(true);
   m_pItemsSelect[16]->disableClick();
   m_IndexRCInputType = addMenuItem(m_pItemsSelect[16]);


   if ( NULL != g_pCurrentModel )
      log_line("Primary HID for this vehicle: HID id: %u", g_pCurrentModel->rc_params.hid_id);

   m_pItemsSelect[15] = new MenuItemSelect("Primary input HID", "Sets the primary HID (joysticks/gamepad/RC transmitter) used for this vehicle.");  

   m_nIndexPrimaryHID = -1;
   m_pJoystick = NULL;
   m_TimeLastJoystickCheck = 0;

   for( int i=0; i<pCI->inputInterfacesCount; i++ )
   {
      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
      if ( NULL == pCII )
         continue;
      m_pItemsSelect[15]->addSelection(pCII->szInterfaceName);
      if ( NULL != g_pCurrentModel && pCII->uId == g_pCurrentModel->rc_params.hid_id )
      {
         log_line("Found primary HID for this vehicle: HID id: %u", pCII->uId);
         m_pItemsSelect[15]->setSelectedIndex(i);
         m_nIndexPrimaryHID = i;
         m_pJoystick = pCII;
         hardware_open_joystick( m_pJoystick->currentHardwareIndex );
      }
   }
   m_pItemsSelect[15]->setIsEditable();
   m_IndexHIDPrimary = addMenuItem(m_pItemsSelect[15]);
   if ( 0 == pCI->inputInterfacesCount )
   {
      m_pItemsSelect[15]->addSelection("None Detected");
      m_pItemsSelect[15]->setEnabled(false);
   }
   else if ( -1 == m_nIndexPrimaryHID )
   {
      m_pItemsSelect[15]->addSelection("Unknown");
      m_pItemsSelect[15]->setSelectedIndex(m_pItemsSelect[15]->getSelectionsCount()-1);
   }

   m_pItemsSelect[1] = new MenuItemSelect("RC Output Channels Count", "Sets how many RC channels to send to the flight controller.");
   m_pItemsSelect[1]->addSelection("2");
   m_pItemsSelect[1]->addSelection("4");
   m_pItemsSelect[1]->addSelection("6");
   m_pItemsSelect[1]->addSelection("8");
   m_pItemsSelect[1]->addSelection("10");
   m_pItemsSelect[1]->addSelection("12");
   m_pItemsSelect[1]->addSelection("14");
   m_pItemsSelect[1]->addSelection("16");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexChannelsCount = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("RC Rate", "Frequency of the RC frames sent to vehicle");
   m_pItemsSelect[2]->addSelection("200 ms");
   m_pItemsSelect[2]->addSelection("100 ms");
   m_pItemsSelect[2]->addSelection("70 ms");
   m_pItemsSelect[2]->addSelection("50 ms");
   m_pItemsSelect[2]->addSelection("40 ms");
   m_pItemsSelect[2]->addSelection("35 ms");
   m_pItemsSelect[2]->addSelection("30 ms");
   m_pItemsSelect[2]->addSelection("25 ms");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexRCFrames = addMenuItem(m_pItemsSelect[2]);

   m_IndexChannels = addMenuItem(new MenuItem("Channels Assignment", "Change which RC channels are assigned to which sticks, joysticks and buttons."));
   m_pMenuItems[m_IndexChannels]->showArrow();

   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_CAR || g_pCurrentModel->vehicle_type == MODEL_TYPE_BOAT || g_pCurrentModel->vehicle_type == MODEL_TYPE_ROBOT )
   {
      m_pItemsSelect[5] = new MenuItemSelect("Throttle Reverse/Forward", "Assign a button or switch to change throttle from forward to reverse and vice versa.");
      m_pItemsSelect[5]->addSelection("None");
      //m_pItemsSelect[5]->setIsEditable();
      m_IndexThrotleReverse = addMenuItem(m_pItemsSelect[5]);
   }

   m_IndexExpo = addMenuItem(new MenuItem("Channels Expo", "Change the expo settings for each channel."));
   m_pMenuItems[m_IndexExpo]->showArrow();

   m_IndexRCCamera = addMenuItem(new MenuItem("Camera Control", "Assign and configure RC channels for camera control."));
   m_pMenuItems[m_IndexRCCamera]->showArrow();

   char szBuff[256];
   
   addMenuItem(new MenuItemSection("Failsafe Settings"));

   m_pItemsSlider[0] = new MenuItemSlider("Failsafe trigger time (ms)", "Sets the timeout (in miliseconds) for triggering a RC failsafe when link is lost.", 50,3000,1000, fSliderWidth);
   m_pItemsSlider[0]->setStep(50);
   m_IndexFailsafeTime = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[4] = new MenuItemSelect("Failsafe Type", "Sets what RC output will be generated when a failsafe event occurs.");
   m_pItemsSelect[4]->addSelection("No output");
   m_pItemsSelect[4]->addSelection("Keep last");
   m_pItemsSelect[4]->addSelection("Below range");
   m_pItemsSelect[4]->addSelection("Custom");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexFailsafeType = addMenuItem(m_pItemsSelect[4]);

   m_IndexFailsafeValues = addMenuItem(new MenuItem("Custom Failsafe Values", "Change the values each RC channel will output when in failsafe mode."));
   m_pMenuItems[m_IndexFailsafeValues]->showArrow();

   Preferences* p = get_Preferences();
   m_iQA1Org = p->iActionQuickButton1;
   m_iQA2Org = p->iActionQuickButton2;
   m_iQA3Org = p->iActionQuickButton3;

   m_ExtraItemsHeight = 0.0;

   m_TimeLastRCCompute = g_TimeNow;
}

MenuVehicleRC::~MenuVehicleRC()
{
   if ( NULL != m_pJoystick )
      hardware_close_joystick( m_pJoystick->currentHardwareIndex );
   m_pJoystick = NULL;
}


void MenuVehicleRC::onShow()
{
   Menu::onShow();
   m_TimeLastRCCompute = g_TimeNow;
   controllerInterfacesEnumJoysticks();
}

int MenuVehicleRC::onBack()
{
   if ( m_bWaitingForInput )
   {
      m_bWaitingForInput = false;

      if ( NULL != m_pPopupAssignment )
      {
         popups_remove(m_pPopupAssignment);
         delete m_pPopupAssignment;
         m_pPopupAssignment = NULL;
      }

      rc_parameters_t params;
      memcpy((u8*)&params, (u8*)&(g_pCurrentModel->rc_params), sizeof(rc_parameters_t));

      params.rcChAssignmentThrotleReverse = 0;      

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();

      m_pPopupAssignment = new Popup(true, "Channel assignment canceled", 3 );
      m_pPopupAssignment->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(m_pPopupAssignment);

      return 1;
   }

   return Menu::onBack();
}


void MenuVehicleRC::updateUIState(bool bEnable)
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   m_pItemsSelect[1]->setEnabled(bEnable);
   m_pItemsSelect[2]->setEnabled(bEnable);
   m_pItemsSlider[0]->setEnabled(bEnable);
   m_pMenuItems[m_IndexChannels]->setEnabled(bEnable);
   m_pMenuItems[m_IndexExpo]->setEnabled(bEnable);
   m_pMenuItems[m_IndexRCCamera]->setEnabled(bEnable);
   m_pItemsSelect[4]->setEnabled(bEnable);
   m_pMenuItems[m_IndexFailsafeValues]->setEnabled(bEnable && (m_pItemsSelect[4]->getSelectedIndex() == 3));
   m_pItemsSelect[10]->setEnabled(bEnable);
   m_pItemsSelect[15]->setEnabled(bEnable);
   if ( 0 == pCI->inputInterfacesCount || (g_pCurrentModel->rc_params.inputType != RC_INPUT_TYPE_USB) )
      m_pItemsSelect[15]->setEnabled(false);
   m_pItemsSelect[16]->setEnabled(bEnable);

   if ( -1 != m_IndexThrotleReverse )
      m_pItemsSelect[5]->setEnabled(bEnable);
}

void MenuVehicleRC::valuesToUI()
{
   Preferences* p = get_Preferences();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   int nOSDIndex = g_pCurrentModel->osd_params.layout;
   if ( nOSDIndex < 0 || nOSDIndex >= MODEL_MAX_OSD_PROFILES )
      nOSDIndex = 0;

   m_pItemsSelect[0]->setSelection((int)(g_pCurrentModel->rc_params.rc_enabled));

   int index = g_pCurrentModel->rc_params.channelsCount;
   if ( index < 2 )
      index = 0;
   else
      index = (index-2)/2;
   if ( index > 7 )
      index = 7;
   m_pItemsSelect[1]->setSelection(index);

   m_pItemsSelect[2]->setSelection(g_pCurrentModel->rc_params.rc_frames_per_second/5-1);

   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->rc_params.rc_failsafe_timeout_ms);

   int fsMode = g_pCurrentModel->rc_params.failsafeFlags & 0xFF;
   if ( fsMode == RC_FAILSAFE_CUSTOM )
      fsMode = 3;
   m_pItemsSelect[4]->setSelection(fsMode);

   m_pItemsSelect[10]->setSelectedIndex((g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED)?1:0);
   if ( p->iActionQuickButton1 == quickActionRCEnable )
      m_pItemsSelect[10]->setSelectedIndex(2);
   if ( p->iActionQuickButton2 == quickActionRCEnable )
      m_pItemsSelect[10]->setSelectedIndex(3);
   if ( p->iActionQuickButton3 == quickActionRCEnable )
      m_pItemsSelect[10]->setSelectedIndex(4);

   for( int i=0; i<pCI->inputInterfacesCount; i++ )
   {
      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
      if ( NULL == pCII )
         continue;
      if ( NULL != g_pCurrentModel && pCII->uId == g_pCurrentModel->rc_params.hid_id )
      {
         log_line("Update UI: Found primary HID for this vehicle: HID id: %u", pCII->uId);
         m_pItemsSelect[15]->setSelectedIndex(i);
      }
   }
   m_pItemsSelect[16]->setSelectedIndex(g_pCurrentModel->rc_params.inputType);

   if ( -1 != m_IndexThrotleReverse )
   {
      m_pItemsSelect[5]->removeAllSelections();
      if ( ! (g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) )
         m_pItemsSelect[5]->addSelection("None");
      else
      {
         char szBuff[128];
         strcpy(szBuff, "None");
         if ( g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_BUTTON )
         {
            szBuff[0] = 0;
            for( int shift = 4; shift <= 24; shift += 4 )
            {
               if ( ((g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse >> shift) & 0x0F) > 0 )
               if ( strlen(szBuff)<20 )
               {
                  char szTmp[32];
                  if ( 0 == szBuff[0] )
                     sprintf(szBuff, "B-%d", ((g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse >> shift) & 0x0F));
                  else
                  {
                     sprintf(szTmp, ", B-%d", ((g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse >> shift) & 0x0F));
                     strcat(szBuff, szTmp);
                  }
               }
            }
         }
         else if ( ((g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse >> 4 ) & 0x0F) > 0 )
            sprintf(szBuff, "A-%d", (((g_pCurrentModel->rc_params.rcChAssignmentThrotleReverse) >>4 ) & 0x0F));
         m_pItemsSelect[5]->addSelection(szBuff);
      }
      m_pItemsSelect[5]->setSelectedIndex(0);
   }

   updateUIState( g_pCurrentModel->rc_params.rc_enabled );

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_NONE || g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_RC_IN_SBUS_IBUS)
   {
      if ( -1 != m_IndexThrotleReverse )
         m_pMenuItems[5]->setEnabled(false);
      m_pMenuItems[m_IndexChannels]->setEnabled(false);
      m_pMenuItems[m_IndexExpo]->setEnabled(false);
   }

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_NONE )
      m_pMenuItems[m_IndexRCCamera]->setEnabled(false);
}


bool MenuVehicleRC::periodicLoop()
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_NONE )
      return false;

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB && (0 == pCI->inputInterfacesCount) )
      return false;

   if ( m_bIsAnimationInProgress )
      return false;

   if ( ! menu_is_menu_on_top(this) )
      return false;
   hw_joystick_info_t* pJoy = NULL;

   u32 timeNow = g_TimeNow;
   u32 miliSec = timeNow - m_TimeLastRCCompute;
   m_TimeLastRCCompute = timeNow;

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
   {
      if ( -1 == m_nIndexPrimaryHID || NULL == m_pJoystick )
      {
         for( int i=0; i<pCI->inputInterfacesCount; i++ )
         {
            t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
            if ( NULL == pCII )
               continue;
            if ( NULL != g_pCurrentModel && pCII->uId == g_pCurrentModel->rc_params.hid_id )
            {
               log_line("Found primary HID for this vehicle: HID id: %u", pCII->uId);
               m_nIndexPrimaryHID = i;
               m_pJoystick = pCII;
               hardware_open_joystick( m_pJoystick->currentHardwareIndex );
            }
         }
      }

      if ( -1 == m_nIndexPrimaryHID || NULL == m_pJoystick )
         return false;

      pJoy = hardware_get_joystick_info(m_pJoystick->currentHardwareIndex);
      if ( NULL == pJoy )
      {
         for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
            m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], &g_SM_RCIn, NULL, NULL, miliSec);
         return false;
      }
      if ( 0 == hardware_is_joystick_opened(m_pJoystick->currentHardwareIndex) )
      {
         for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
            m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], &g_SM_RCIn, NULL, NULL, miliSec);

         if ( g_TimeNow > m_TimeLastJoystickCheck + 500 )
         {
            m_TimeLastJoystickCheck = g_TimeNow;
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
      for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
      {
         m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], &g_SM_RCIn, pJoy, m_pJoystick, miliSec);
      }
   }

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
   {
      for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
      {
         m_CurrentRCValues[i] = compute_controller_rc_value(g_pCurrentModel, i, m_CurrentRCValues[i], &g_SM_RCIn, NULL, NULL, miliSec);
      }
   }

   if ( m_bWaitingForInput && NULL != pJoy && NULL != m_pJoystick )
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

void MenuVehicleRC::onAssignButton(int buttonIndex)
{
   if ( !m_bWaitingForInput )
      return;

   if ( NULL != m_pPopupAssignment )
   {
      popups_remove(m_pPopupAssignment);
      delete m_pPopupAssignment;
      m_pPopupAssignment = NULL;
   }

   char szBuff[64];
   sprintf(szBuff, "Assigned button %d to throttle reverse/fwd", buttonIndex+1);
   m_pPopupAssignment = new Popup(true, szBuff, 3 );
   m_pPopupAssignment->setIconId(g_idIconInfo, get_Color_IconNormal());
   popups_add_topmost(m_pPopupAssignment);

   rc_parameters_t params;
   memcpy((u8*)&params, (u8*)&(g_pCurrentModel->rc_params), sizeof(rc_parameters_t));

   u32 button = buttonIndex+1;
   if ( button > 15 )
      button = 15;

   // No assignment?
   if ( ! (params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) )
   {
      params.rcChAssignmentThrotleReverse = RC_CH_ASSIGNMENT_FLAG_ASSIGNED | RC_CH_ASSIGNMENT_FLAG_BUTTON;
      params.rcChAssignmentThrotleReverse |= (button << 4);
   }
   // Was assigned to an axe?
   else if ( (params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) &&
             (!(params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_BUTTON)))
   {
      params.rcChAssignmentThrotleReverse = RC_CH_ASSIGNMENT_FLAG_ASSIGNED | RC_CH_ASSIGNMENT_FLAG_BUTTON;
      params.rcChAssignmentThrotleReverse |= (button << 4);
   }
   // Was assigned to a button
   else
   {
      log_line("Assign multibutton: Add button: %d", button);
      params.rcChAssignmentThrotleReverse |= RC_CH_ASSIGNMENT_FLAG_MULTIPLE;
      for( int shift = 4; shift<=24; shift += 4 )
      {
         if ( ((params.rcChAssignmentThrotleReverse >> shift) & 0x0F) == 0 )
         {
            u32 addBits = (button << shift);
            params.rcChAssignmentThrotleReverse |= addBits;
            log_line("Assign multibutton: Added button: %d to position %d shift", button, shift);
            break;
         }
      }
   }

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();

   m_bWaitingForInput = false;
}

void MenuVehicleRC::onAssignAxe(int axeIndex)
{
   if ( !m_bWaitingForInput )
      return;

   if ( NULL != m_pPopupAssignment )
   {
      popups_remove(m_pPopupAssignment);
      delete m_pPopupAssignment;
      m_pPopupAssignment = NULL;
   }

   char szBuff[64];
   sprintf(szBuff, "Assigned axe %d to throttle reverse/fwd", axeIndex+1);
   m_pPopupAssignment = new Popup(true, szBuff, 3 );
   m_pPopupAssignment->setIconId(g_idIconInfo, get_Color_IconNormal());
   popups_add_topmost(m_pPopupAssignment);

   rc_parameters_t params;
   memcpy((u8*)&params, (u8*)&(g_pCurrentModel->rc_params), sizeof(rc_parameters_t));

   u32 axe = axeIndex+1;
   if ( axe > 15 )
      axe = 15;
   params.rcChAssignmentThrotleReverse = RC_CH_ASSIGNMENT_FLAG_ASSIGNED;
   params.rcChAssignmentThrotleReverse |= (axe << 4);

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();

   m_bWaitingForInput = false;
}

void MenuVehicleRC::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);

   renderLiveValues();
}

void MenuVehicleRC::renderLiveValues()
{
   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float fPaddingY = m_sfMenuPaddingY;
   float fPaddingX = m_sfMenuPaddingX;

   float fItemSpacing = MENU_ITEM_SPACING *height_text;

   float xPos = m_RenderXPos + m_RenderWidth + 0.02;
   float yPos = m_RenderYPos + 0.02;
   float fWidth = 0.3 * m_sfScaleFactor;
   float fHeight = 2.0*fPaddingY + height_text*5.5 + g_pCurrentModel->rc_params.channelsCount * (height_text + fItemSpacing);

   g_pRenderEngine->setColors(get_Color_PopupBg());
   g_pRenderEngine->setStroke(get_Color_PopupBorder());

   g_pRenderEngine->drawRoundRect(xPos, yPos, fWidth, fHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

   g_pRenderEngine->setColors(get_Color_MenuText());
   
   float y = yPos + fPaddingY;
   xPos += fPaddingX;
   fWidth -= fPaddingX;

   sprintf(szBuff, "RC Output: %s", (g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED)?"Enabled":"Disabled");
   g_pRenderEngine->drawText(xPos, y, g_idFontMenu, szBuff);
   y += height_text;

   bool bIsFailSafe = false;

   u8 fsMode = g_pCurrentModel->rc_params.failsafeFlags & 0xFF;

   if ( RC_FAILSAFE_NOOUTPUT == fsMode )
      sprintf(szBuff, "RC Failsafe Type: No output");
   if ( RC_FAILSAFE_KEEPLAST == fsMode )
      sprintf(szBuff, "RC Failsafe Type: Keep last values");
   if ( RC_FAILSAFE_BELOWRANGE == fsMode )
      sprintf(szBuff, "RC Failsafe Type: Output below range");
   if ( RC_FAILSAFE_VALUE == fsMode )
      sprintf(szBuff, "RC Failsafe Type: Fixed values");
   if ( RC_FAILSAFE_CUSTOM == fsMode )
      sprintf(szBuff, "RC Failsafe Type: Custom");
   g_pRenderEngine->drawText(xPos, y, g_idFontMenu, szBuff);
   y += height_text;

   sprintf(szBuff, "Input: Ok");
   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_NONE )
   {
      sprintf(szBuff, "Input: No Input Source");
      bIsFailSafe = true;
   }

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
   {
      if ( -1 == m_nIndexPrimaryHID || NULL == m_pJoystick )
         bIsFailSafe = true;
   }

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
   if ( NULL != g_pSM_RCIn )
   if ( ! (g_SM_RCIn.uFlags & RC_IN_FLAG_HAS_INPUT) )
   {
      sprintf(szBuff, "Input: No SBUS/IBUS frames");
      bIsFailSafe = true;
   }

   g_pRenderEngine->drawText(xPos, y, g_idFontMenu, szBuff);
   y += height_text*1.8;

   float x0 = xPos;
   float x1 = xPos + 0.07*m_sfScaleFactor;
   float x2 = xPos + 0.15*m_sfScaleFactor;
   float x3 = xPos + 0.23*m_sfScaleFactor;
   g_pRenderEngine->drawText(x0, y, g_idFontMenu, "Channel");
   g_pRenderEngine->drawText(x1, y, g_idFontMenu, "Live Value");
   g_pRenderEngine->drawText(x2-0.01*m_sfScaleFactor, y, g_idFontMenu, "FailSafe Type");
   if ( g_pCurrentModel->rc_params.inputType != RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
      g_pRenderEngine->drawText(x3, y, g_idFontMenu, "Expo");

   y += height_text*1.8;
   g_pRenderEngine->drawLine(x0, y-height_text*0.4, xPos+fWidth-fPaddingX, y-height_text*0.4);

   float corner = 0.003*m_sfScaleFactor;
   float padding = 0.002*m_sfScaleFactor+g_pRenderEngine->getPixelWidth();
   float rectW = 0.06*m_sfScaleFactor;
   float rectH = m_pMenuItems[0]->getItemHeight(getUsableWidth());

   for( int ch=0; ch<g_pCurrentModel->rc_params.channelsCount; ch++ )
   {
      g_pCurrentModel->get_rc_channel_name(ch, szBuff);
      g_pRenderEngine->drawText(x0, y, g_idFontMenu, szBuff);
      
      u32 fsChType = get_rc_channel_failsafe_type(g_pCurrentModel, ch);
      int fsValue = get_rc_channel_failsafe_value(g_pCurrentModel, ch, m_CurrentRCValues[ch]); 
      if ( (fsValue < 0) || (fsValue > 2500) )
         fsValue = 0;
      int val = m_CurrentRCValues[ch];
      if ( bIsFailSafe )
      {
         val = fsValue;
         if ( RC_FAILSAFE_NOOUTPUT == fsChType )
            val = 0;
         if ( RC_FAILSAFE_BELOWRANGE == fsChType )
            val = fsValue;
      }
      if ( (val < 0) || (val > 2500) )
         val = 0;
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,100,0,0.8);
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->setStrokeSize(0);
      if ( (val >= 0) && (val < 2500) )
         g_pRenderEngine->drawRoundRect(x1+padding, y, (rectW-2*padding)*(float)(val-g_pCurrentModel->rc_params.rcChMin[ch])/(g_pCurrentModel->rc_params.rcChMax[ch]-g_pCurrentModel->rc_params.rcChMin[ch]), rectH, corner);

      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(x1, y, rectW, rectH, corner);
   
      g_pRenderEngine->setColors(get_Color_MenuText());
      sprintf(szBuff, "%d", val);
      if ( bIsFailSafe )
      {
         u32 fsChType = get_rc_channel_failsafe_type(g_pCurrentModel, ch);
         sprintf(szBuff, "FS: %d", val);
         if ( RC_FAILSAFE_NOOUTPUT == fsChType )
            sprintf(szBuff, "FS ---");
      }
      if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
      if ( ! (g_pCurrentModel->rc_params.rcChAssignment[ch] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED ) )
         sprintf(szBuff, "Not Assigned");

      g_pRenderEngine->drawText(x1+0.003*m_sfMenuPaddingX, y-height_text*0.001, g_idFontMenu, szBuff);
      g_pRenderEngine->setColors(get_Color_MenuText());

      sprintf(szBuff, "%d", fsValue);
      if ( RC_FAILSAFE_NOOUTPUT == fsChType )
         sprintf(szBuff, "No output");
      if ( RC_FAILSAFE_BELOWRANGE == fsChType )
         sprintf(szBuff, "Below Range (%d)", fsValue);
      if ( RC_FAILSAFE_KEEPLAST == fsChType )
         sprintf(szBuff, "Keep Last (%d)", fsValue);

      g_pRenderEngine->drawText(x2, y, g_idFontMenu, szBuff);

      if ( g_pCurrentModel->rc_params.inputType != RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
      {
         sprintf(szBuff, "%d%%", g_pCurrentModel->rc_params.rcChExpo[ch]);
         g_pRenderEngine->drawText(x3, y, g_idFontMenu, szBuff);
      }
      y += height_text;
      y += fItemSpacing;
   }
}


void MenuVehicleRC::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( 10 == m_iConfirmationId )
   {
      if ( 1 == returnValue )
         valuesToUI();
      m_iConfirmationId = 0;
      return;
   }

   if ( 2 == m_iConfirmationId )
   {
      if ( 1 != returnValue )
      {
         m_iConfirmationId = 0;
         valuesToUI();
         return;
      }
      m_iConfirmationId = 0;

      rc_parameters_t params;

      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.rc_enabled = m_bPendingEnable;

      if ( pCI->inputInterfacesCount > 0 )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(m_pItemsSelect[15]->getSelectedIndex());
         if ( pCII != NULL )
            params.hid_id = pCII->uId;
      }

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();

      updateUIState(m_bPendingEnable);
      return;
   }
   m_iConfirmationId = 0;
}

void MenuVehicleRC::onSelectItem()
{
   if ( m_bWaitingForInput )
      return;

   if ( m_IndexRCInputType == m_SelectedIndex && (! m_pMenuItems[m_SelectedIndex]->isEditing()) )
   {
      m_iConfirmationId = 10;
      add_menu_to_stack(new MenuVehicleRCInput());
      return;
   }

   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   Preferences* p = get_Preferences();
   int nOSDIndex = g_pCurrentModel->osd_params.layout;
   if ( nOSDIndex < 0 || nOSDIndex >= MODEL_MAX_OSD_PROFILES )
      nOSDIndex = 0;

   if ( m_IndexRCEnabled == m_SelectedIndex )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      bool enable = (index!=0);
      if ( enable == g_pCurrentModel->rc_params.rc_enabled )
         return;

      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         m_bPendingEnable = enable;
         char szBuff[256];
         if ( ! enable )
            sprintf(szBuff, "Your vehicle is armed. Are you sure you want to disable the RC link? You will lose RC controll of the vehicle.");
         else
            sprintf(szBuff, "Your vehicle is armed. Are you sure you want to enable the RC link? It can have unintended consequences depending on how the flight controller is setup.");
         m_iConfirmationId = 2;
         MenuConfirmation* pMC = new MenuConfirmation("WARNING!",szBuff,m_iConfirmationId);
         add_menu_to_stack(pMC);         
         return;
      }

      rc_parameters_t params;

      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.rc_enabled = enable;

      if ( pCI->inputInterfacesCount > 0 )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(m_pItemsSelect[15]->getSelectedIndex());
         if ( pCII != NULL )
            params.hid_id = pCII->uId;
      }

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();

      updateUIState(enable);
   }

   if ( m_IndexHIDPrimary == m_SelectedIndex )
   {
      if ( m_pItemsSelect[15]->getSelectedIndex() >= pCI->inputInterfacesCount )
         return;

      if ( NULL != m_pJoystick )
         hardware_close_joystick(m_pJoystick->currentHardwareIndex);

      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(m_pItemsSelect[15]->getSelectedIndex());
      if ( NULL == pCII )
         return;

      m_nIndexPrimaryHID = m_pItemsSelect[15]->getSelectedIndex();
      m_pJoystick = pCII;
      hardware_open_joystick( m_pJoystick->currentHardwareIndex );

      if ( pCII->uId == g_pCurrentModel->rc_params.hid_id )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.hid_id = pCII->uId;
     
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexEnableOutput == m_SelectedIndex )
   {
      int index = m_pItemsSelect[10]->getSelectedIndex();
      int enabled = m_pItemsSelect[10]->getSelectedIndex();

      int current = (g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED)?1:0;
      if ( (enabled == 0 || enabled == 1) && enabled == current )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

      if ( enabled == 1 )
         params.flags |= RC_FLAGS_OUTPUT_ENABLED;
      if ( enabled == 0 )
         params.flags &= (~RC_FLAGS_OUTPUT_ENABLED);

      if ( enabled == 0 || enabled == 1 )
      {
         if ( p->iActionQuickButton1 == quickActionRCEnable )
            p->iActionQuickButton1 = m_iQA1Org;
         if ( p->iActionQuickButton2 == quickActionRCEnable )
            p->iActionQuickButton2 = m_iQA2Org;
         if ( p->iActionQuickButton3 == quickActionRCEnable )
            p->iActionQuickButton3 = m_iQA3Org;

         if ( p->iActionQuickButton1 == quickActionRCEnable )
            p->iActionQuickButton1 = quickActionVideoRecord;
         if ( p->iActionQuickButton2 == quickActionRCEnable )
            p->iActionQuickButton2 = quickActionToggleAllOff;
         if ( p->iActionQuickButton3 == quickActionRCEnable )
            p->iActionQuickButton3 = quickActionTakePicture;

         save_Preferences();      

         log_dword("Sending RC flags: ", g_pCurrentModel->rc_params.flags);
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
            valuesToUI();
      }
      else
      {
         p->iActionQuickButton1 = m_iQA1Org;
         p->iActionQuickButton2 = m_iQA2Org;
         p->iActionQuickButton3 = m_iQA3Org;
         if ( p->iActionQuickButton1 == quickActionRCEnable )
            p->iActionQuickButton1 = quickActionVideoRecord;
         if ( p->iActionQuickButton2 == quickActionRCEnable )
            p->iActionQuickButton2 = quickActionToggleAllOff;
         if ( p->iActionQuickButton3 == quickActionRCEnable )
            p->iActionQuickButton3 = quickActionTakePicture;
      
         if ( 2 == index )
            p->iActionQuickButton1 = quickActionRCEnable;
         if ( 3 == index )
            p->iActionQuickButton2 = quickActionRCEnable;
         if ( 4 == index )
            p->iActionQuickButton3 = quickActionRCEnable;
         save_Preferences();
      }
      return;
   }


   if ( m_IndexRCFrames == m_SelectedIndex )
   {
      int fps = (m_pItemsSelect[2]->getSelectedIndex()+1)*5;
      if ( fps == g_pCurrentModel->rc_params.rc_frames_per_second )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.rc_frames_per_second = fps;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();
   }

   if ( m_IndexFailsafeTime == m_SelectedIndex )
   {
      int timeout = m_pItemsSlider[0]->getCurrentValue();
      if ( timeout == g_pCurrentModel->rc_params.rc_failsafe_timeout_ms )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.rc_failsafe_timeout_ms = timeout;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();
   }

   if ( m_IndexFailsafeType == m_SelectedIndex )
   {
      int index = m_pItemsSelect[4]->getSelectedIndex();
      if ( index == 3 )
         index = RC_FAILSAFE_CUSTOM;
      if ( index == (g_pCurrentModel->rc_params.failsafeFlags & 0xFF) )
         return;
      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.failsafeFlags = ( params.failsafeFlags & (~0xFF) ) | (index & 0xFF);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();
   }


   if ( m_IndexChannelsCount == m_SelectedIndex )
   {
      int index = m_pItemsSelect[1]->getSelectedIndex();
      int indexOld = g_pCurrentModel->rc_params.channelsCount;
      if ( indexOld < 2 )
         indexOld = 0;
      else
         indexOld = (indexOld-2)/2;
      if ( indexOld > 7 )
         indexOld = 7;

      if ( index == indexOld )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      params.channelsCount = index*2+2;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();
   }

   if ( m_IndexChannels == m_SelectedIndex )
   {
      if ( NULL != m_pJoystick )
         hardware_close_joystick( m_pJoystick->currentHardwareIndex );
      add_menu_to_stack(new MenuVehicleRCChannels());
   }

   if ( m_IndexThrotleReverse == m_SelectedIndex )
   {
      log_line("Click on assigning throttle reverse/fwd button");
      if ( -1 == m_nIndexPrimaryHID || NULL == m_pJoystick )
         return;

      hw_joystick_info_t* pJoy = hardware_get_joystick_info(m_pJoystick->currentHardwareIndex);
      if ( NULL == pJoy )
         return;
      for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
         m_AxesBeforeAssign[i] = pJoy->axesValues[i];
      for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
         m_ButtonsBeforeAssign[i] = pJoy->buttonsValues[i];
      m_bWaitingForInput = true;
      log_line("Start waiting for assigning throttle reverse/fwd button");

      if ( NULL != m_pPopupAssignment )
      {
         popups_remove(m_pPopupAssignment);
         delete m_pPopupAssignment;
         m_pPopupAssignment = NULL;
      }

      m_pPopupAssignment = new Popup(true, "Assign button to throttle reverse/forward", 60 );
      m_pPopupAssignment->setIconId(g_idIconInfo, get_Color_IconNormal());
      m_pPopupAssignment->addLine("Press the button/switch you want to assign for throttle reverse/forward.");
      m_pPopupAssignment->addLine("Press [Cancel/Back] key to remove the assignment.");
      popups_add_topmost(m_pPopupAssignment);
   }

   if ( m_IndexExpo == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleRCExpo());
   }
   if ( m_IndexRCCamera == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleRCCamera());
   }
   if ( m_IndexFailsafeValues == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleRCFailsafe());
   }
}