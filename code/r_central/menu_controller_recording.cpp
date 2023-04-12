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
#include "menu.h"
#include "menu_controller_recording.h"
#include "shared_vars.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "popup_log.h"
#include "colors.h"
#include "osd_common.h"

MenuControllerRecording::MenuControllerRecording(void)
:Menu(MENU_ID_CONTROLLER_RECORDING, "Video Recording Settings", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   int c = 0;

   m_pItemsSelect[c] = new MenuItemSelect("Add OSD To Screenshots", "When taking a screenshot, OSD info can be included in the picture or not.");  
   m_pItemsSelect[c]->addSelection("No");
   m_pItemsSelect[c]->addSelection("Yes");
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Recording buffers", "When recording, record directly to the persistent storage or to memory first. Record to memory first when having slow performance on the persistent storage. Either way, when video recording ends, the video is saved to persistent storage.");  
   m_pItemsSelect[c]->addSelection("Storage");
   m_pItemsSelect[c]->addSelection("Memory");
   m_IndexVideoDestination = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Record Indicator Style", "Select which style of record indicator to show on the OSD");  
   m_pItemsSelect[c]->addSelection("Normal");
   m_pItemsSelect[c]->addSelection("Large");
   m_IndexRecordIndicator = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Recording LED", "Select what the recording LED should do when recording is on.");  
   m_pItemsSelect[c]->addSelection("Disabled");
   m_pItemsSelect[c]->addSelection("On/Off");
   m_pItemsSelect[c]->addSelection("Blinking");
   m_IndexRecordLED = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Record Button", "Select which button to use to trigger start/stop of video recording.");  
   m_pItemsSelect[c]->addSelection("None");
   m_pItemsSelect[c]->addSelection("QA Button 1");
   m_pItemsSelect[c]->addSelection("QA Button 2");
   m_pItemsSelect[c]->addSelection("QA Button 3");
   m_IndexRecordButton = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Start recording on Arm", "Starts video recording (if it's not started already) when the vehicle arms. It requires an active connection of the vehicle to the flight controller.");
   m_pItemsSelect[c]->addSelection("No");
   m_pItemsSelect[c]->addSelection("Yes");
   m_IndexRecordArm = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect("Stop recording on Disarm", "Stops video recording (if one is started) when the vehicle disarms. It requires an active connection of the vehicle to the flight controller.");  
   m_pItemsSelect[c]->addSelection("No");
   m_pItemsSelect[c]->addSelection("Yes");
   m_IndexRecordDisarm = addMenuItem(m_pItemsSelect[c]);
   c++;
}

void MenuControllerRecording::valuesToUI()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   m_pItemsSelect[0]->setSelection(p->iAddOSDOnScreenshots);
   //m_pItemsSelect[0]->setEnabled((p->iActionQuickButton1==quickActionTakePicture) || (p->iActionQuickButton2==quickActionTakePicture));

   m_pItemsSelect[1]->setSelection(p->iVideoDestination);
  
   m_pItemsSelect[2]->setSelection(p->iShowBigRecordButton);
   m_pItemsSelect[3]->setSelection(p->iRecordingLedAction);

   m_pItemsSelect[4]->setSelection(0);
   if ( p->iActionQuickButton1 == quickActionVideoRecord )
      m_pItemsSelect[4]->setSelection(1);
   if ( p->iActionQuickButton2 == quickActionVideoRecord )
      m_pItemsSelect[4]->setSelection(2);
   if ( p->iActionQuickButton3 == quickActionVideoRecord )
      m_pItemsSelect[4]->setSelection(3);

   m_pItemsSelect[5]->setSelection(p->iStartVideoRecOnArm);
   m_pItemsSelect[6]->setSelection(p->iStopVideoRecOnDisarm);
}

void MenuControllerRecording::onShow()
{
   removeAllTopLines();
   Menu::onShow();
}

void MenuControllerRecording::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerRecording::onSelectItem()
{
   Menu::onSelectItem();

   Preferences* p = get_Preferences();
   if ( NULL == p )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( 0 == m_SelectedIndex )
      p->iAddOSDOnScreenshots = m_pItemsSelect[0]->getSelectedIndex();

   if ( 1 == m_SelectedIndex )
      p->iVideoDestination = m_pItemsSelect[1]->getSelectedIndex();

   if ( m_IndexRecordIndicator == m_SelectedIndex )
   {
      p->iShowBigRecordButton =  m_pItemsSelect[2]->getSelectedIndex();
      save_Preferences();
   }

   if ( m_IndexRecordLED == m_SelectedIndex )
   {
      p->iRecordingLedAction =  m_pItemsSelect[3]->getSelectedIndex();
      save_Preferences();
   }


   if ( m_IndexRecordButton == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         if ( p->iActionQuickButton1 == quickActionVideoRecord )
            p->iActionQuickButton1 = quickActionTakePicture;
         if ( p->iActionQuickButton2 == quickActionVideoRecord )
            p->iActionQuickButton2 = quickActionTakePicture;
         if ( p->iActionQuickButton3 == quickActionVideoRecord )
            p->iActionQuickButton3 = quickActionTakePicture;
      }
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         p->iActionQuickButton1 = quickActionVideoRecord;
         if ( p->iActionQuickButton2 == quickActionVideoRecord )
            p->iActionQuickButton2 = quickActionTakePicture;
         if ( p->iActionQuickButton3 == quickActionVideoRecord )
            p->iActionQuickButton3 = quickActionTakePicture;
      }
      if ( 2 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         p->iActionQuickButton2 = quickActionVideoRecord;
         if ( p->iActionQuickButton1 == quickActionVideoRecord )
            p->iActionQuickButton1 = quickActionTakePicture;
         if ( p->iActionQuickButton3 == quickActionVideoRecord )
            p->iActionQuickButton3 = quickActionTakePicture;
      }
      if ( 3 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         p->iActionQuickButton3 = quickActionVideoRecord;
         if ( p->iActionQuickButton1 == quickActionVideoRecord )
            p->iActionQuickButton1 = quickActionTakePicture;
         if ( p->iActionQuickButton2 == quickActionVideoRecord )
            p->iActionQuickButton2 = quickActionTakePicture;
      }

      save_Preferences();
   }

   if ( m_IndexRecordArm == m_SelectedIndex )
   {
      p->iStartVideoRecOnArm =  m_pItemsSelect[5]->getSelectedIndex();
      save_Preferences();
   }

   if ( m_IndexRecordDisarm == m_SelectedIndex )
   {
      p->iStopVideoRecOnDisarm =  m_pItemsSelect[6]->getSelectedIndex();
      save_Preferences();
   }

   save_Preferences();
   apply_Preferences();
}
