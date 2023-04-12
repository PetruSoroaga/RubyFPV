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
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ctrl_settings.h"
#include "menu.h"
#include "menu_preferences_buttons.h"
#include "shared_vars.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "popup_log.h"
#include "colors.h"
#include "osd_common.h"
#include "menu_preferences_ui.h"

MenuPreferences::MenuPreferences(void)
:Menu(MENU_ID_PREFERENCES, "User Interface Buttons", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   int c = 0;

   m_pItemsSelect[c] = new MenuItemSelect("Swap +/- Buttons", "Swaps the function of + and - buttons.");  
   m_pItemsSelect[c]->addSelection("Off");
   m_pItemsSelect[c]->addSelection("On");
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Swap +/- Edit Buttons", "Swaps the function of + and - buttons for editing values in UI.");  
   m_pItemsSelect[c]->addSelection("Off");
   m_pItemsSelect[c]->addSelection("On");
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Quick Action Button 1", "Change what happens when you press the quick action button 1.");  
   m_pItemsSelect[c]->addSelection("None");
   m_pItemsSelect[c]->addSelection("Cycle OSD screen");
   m_pItemsSelect[c]->addSelection("Cycle OSD size");
   m_pItemsSelect[c]->addSelection("Take Picture");
   m_pItemsSelect[c]->addSelection("Video Recording");
   m_pItemsSelect[c]->addSelection("Toggle OSD Off");
   m_pItemsSelect[c]->addSelection("Toggle Stats Off");
   m_pItemsSelect[c]->addSelection("Toggle All Off");
   m_pItemsSelect[c]->addSelection("Relay Switch");
   m_pItemsSelect[c]->addSelection("Camera Profile");
   m_pItemsSelect[c]->addSelection("RC Output On/Off");
   m_pItemsSelect[c]->addSelection("Rotary Encoder Function");
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Quick Action Button 2", "Change what happens when you press the quick action button 2.");  
   m_pItemsSelect[c]->addSelection("None");
   m_pItemsSelect[c]->addSelection("Cycle OSD screen");
   m_pItemsSelect[c]->addSelection("Cycle OSD size");
   m_pItemsSelect[c]->addSelection("Take Picture");
   m_pItemsSelect[c]->addSelection("Video Recording");
   m_pItemsSelect[c]->addSelection("Toggle OSD Off");
   m_pItemsSelect[c]->addSelection("Toggle Stats Off");
   m_pItemsSelect[c]->addSelection("Toggle All Off");
   m_pItemsSelect[c]->addSelection("Relay Switch");
   m_pItemsSelect[c]->addSelection("Camera Profile");
   m_pItemsSelect[c]->addSelection("RC Output On/Off");
   m_pItemsSelect[c]->addSelection("Rotary Encoder Function");
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Quick Action Button 3", "Change what happens when you press the quick action button 3.");  
   m_pItemsSelect[c]->addSelection("None");
   m_pItemsSelect[c]->addSelection("Cycle OSD screen");
   m_pItemsSelect[c]->addSelection("Cycle OSD size");
   m_pItemsSelect[c]->addSelection("Take Picture");
   m_pItemsSelect[c]->addSelection("Video Recording");
   m_pItemsSelect[c]->addSelection("Toggle OSD Off");
   m_pItemsSelect[c]->addSelection("Toggle Stats Off");
   m_pItemsSelect[c]->addSelection("Toggle All Off");
   m_pItemsSelect[c]->addSelection("Relay Switch");
   m_pItemsSelect[c]->addSelection("Camera Profile");
   m_pItemsSelect[c]->addSelection("RC Output On/Off");
   m_pItemsSelect[c]->addSelection("Rotary Encoder Function");
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Rotary Encoder Function", "Sets the function of the optional rotary encoder. Long press the rotary encoder for 10 seconds to revert to Menu function.");  
   m_pItemsSelect[c]->addSelection("None");
   m_pItemsSelect[c]->addSelection("Menu Navigation");
   m_pItemsSelect[c]->addSelection("Camera Adjustment");
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotaryEncoder = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Rotary Encoder Speed", "Sets the speed of the optional rotary encoder.");  
   m_pItemsSelect[c]->addSelection("Normal");
   m_pItemsSelect[c]->addSelection("Slower");
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotarySpeed = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Rotary Encoder 2 Function", "Sets the function of the secondary optional rotary encoder. Long press the secondary rotary encoder for 10 seconds to revert to Menu function.");  
   m_pItemsSelect[c]->addSelection("None");
   m_pItemsSelect[c]->addSelection("Menu Navigation");
   m_pItemsSelect[c]->addSelection("Camera Adjustment");
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotaryEncoder2 = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Rotary Encoder 2 Speed", "Sets the speed of the secondary optional rotary encoder.");  
   m_pItemsSelect[c]->addSelection("Normal");
   m_pItemsSelect[c]->addSelection("Slower");
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRotarySpeed2 = addMenuItem(m_pItemsSelect[c]);
   c++;
}

void MenuPreferences::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p || NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   m_pItemsSelect[0]->setSelection(p->iSwapUpDownButtons);
   m_pItemsSelect[1]->setSelection(p->iSwapUpDownButtonsValues);
   m_pItemsSelect[2]->setSelection(p->iActionQuickButton1);
   m_pItemsSelect[3]->setSelection(p->iActionQuickButton2);
   m_pItemsSelect[4]->setSelection(p->iActionQuickButton3);

   m_pItemsSelect[5]->setSelection(pCS->nRotaryEncoderFunction);
   m_pItemsSelect[6]->setSelection(pCS->nRotaryEncoderSpeed);

   m_pItemsSelect[7]->setSelection(pCS->nRotaryEncoderFunction2);
   m_pItemsSelect[8]->setSelection(pCS->nRotaryEncoderSpeed2);
}

void MenuPreferences::onShow()
{
   removeAllTopLines();
   Menu::onShow();
}

void MenuPreferences::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuPreferences::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p || NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( 0 == m_SelectedIndex )
   {
      p->iSwapUpDownButtons = m_pItemsSelect[0]->getSelectedIndex();
      hardware_swap_buttons(p->iSwapUpDownButtons);
   }
   if ( 1 == m_SelectedIndex )
   {
      p->iSwapUpDownButtonsValues = m_pItemsSelect[1]->getSelectedIndex();
      hardware_swap_buttons(p->iSwapUpDownButtons);
   }

   if ( 2 == m_SelectedIndex )
   {
      p->iActionQuickButton1 = m_pItemsSelect[2]->getSelectedIndex();
      menu_invalidate_all();
      valuesToUI();
      if ( p->iActionQuickButton1 == quickActionRCEnable )
      {
         Popup* p = new Popup("Enabling/disabling RC link during flight is dangerous. Proceed with caution.", 0.3, 0.3, 0.5, 6 );
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
   }

   if ( 3 == m_SelectedIndex )
   {
      p->iActionQuickButton2 = m_pItemsSelect[3]->getSelectedIndex();
      menu_invalidate_all();
      valuesToUI();
      if ( p->iActionQuickButton2 == quickActionRCEnable )
      {
         Popup* p = new Popup("Enabling/disabling RC link during flight is dangerous. Proceed with caution.", 0.3, 0.3, 0.5, 6 );
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
   }

   if ( 4 == m_SelectedIndex )
   {
      p->iActionQuickButton3 = m_pItemsSelect[4]->getSelectedIndex();
      menu_invalidate_all();
      valuesToUI();
      if ( p->iActionQuickButton3 == quickActionRCEnable )
      {
         Popup* p = new Popup("Enabling/disabling RC link during flight is dangerous. Proceed with caution.", 0.3, 0.3, 0.5, 6 );
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
   }

   if ( m_IndexRotaryEncoder == m_SelectedIndex )
   {
      pCS->nRotaryEncoderFunction = m_pItemsSelect[5]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexRotarySpeed == m_SelectedIndex )
   {
      pCS->nRotaryEncoderSpeed = m_pItemsSelect[6]->getSelectedIndex();
      save_ControllerSettings();
      char szBuff[128];
      sprintf(szBuff, "touch %s", FILE_TMP_I2C_UPDATED);
      hw_execute_bash_command_silent(szBuff, NULL);
      valuesToUI();
      return;
   }

   if ( m_IndexRotaryEncoder2 == m_SelectedIndex )
   {
      pCS->nRotaryEncoderFunction2 = m_pItemsSelect[7]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexRotarySpeed2 == m_SelectedIndex )
   {
      pCS->nRotaryEncoderSpeed2 = m_pItemsSelect[8]->getSelectedIndex();
      save_ControllerSettings();
      char szBuff[128];
      sprintf(szBuff, "touch %s", FILE_TMP_I2C_UPDATED);
      hw_execute_bash_command_silent(szBuff, NULL);
      valuesToUI();
      return;
   }

   save_Preferences();
   apply_Preferences();
}
