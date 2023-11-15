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
#include "../base/encr.h"
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_encryption.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_text.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"

#include "pairing.h"
#include "colors.h"
#include "local_stats.h"
#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "ruby_central.h"
#include "timers.h"

#include <time.h>
#include <sys/resource.h>


MenuControllerEncryption::MenuControllerEncryption(void)
:Menu(MENU_ID_CONTROLLER_ENCRYPTION, "Controller Encryption Settings", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.28;
   m_bHasPassPhrase = false;
   addItems();
}

void MenuControllerEncryption::addItems()
{
   addMenuItem( new MenuItemText("Sets the encryption pass phrase to be used to generate the key and encrypt the radio link.") );
   addMenuItem( new MenuItemText("Pass phrase should be at least 3 words of at least 3 letters each.") );

   char szBuff[128];
 
   int len = 64;
   int res = lpp(szBuff, len);
   if ( res )
   {
      m_bHasPassPhrase = true;
      strcpy(szBuff, "******");
   }
   else
   {
      m_bHasPassPhrase = false;
      strcpy(szBuff, "Not Set");
   }
   m_pItemPass = new MenuItemEdit("Pass phrase", szBuff);
   m_pItemPass->setOnlyLetters();
   m_pItemPass->setMaxLength(MAX_PASS_LENGTH-1);
   m_pItemPass->setIsEditable();
   m_IndexChangePass = addMenuItem(m_pItemPass);

   int countEncr = 0;
   for( int i=0; i<getModelsCount(); i++ )
   {
      Model* pModel = getModelAtIndex(i);
      if ( pModel->enc_flags != MODEL_ENC_FLAGS_NONE )
      if ( m_bHasPassPhrase )
      {
         if ( 0 == countEncr )
            addMenuItem( new MenuItemText("These vehicles currently have encryption enabled:") );
         snprintf(szBuff, sizeof(szBuff), "%s", pModel->getLongName());
         addMenuItem( new MenuItemText(szBuff) );
         countEncr++;
      }
   }
}

void MenuControllerEncryption::valuesToUI()
{
   //ControllerSettings* pCS = get_ControllerSettings();
   //Preferences* pp = get_Preferences();
}

void MenuControllerEncryption::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

int MenuControllerEncryption::onBack()
{
   if ( 2 != m_SelectedIndex )
      return Menu::onBack();

   if ( ! m_pItemPass->isEditing() )
      return Menu::onBack();

   log_line("End edit pp");
   m_pItemPass->endEdit(false);

   char szBuff[MAX_PASS_LENGTH];
   strlcpy(szBuff, m_pItemPass->getCurrentValue(), MAX_PASS_LENGTH);

   m_pItemPass->setCurrentValue("******");
   m_pItemPass->invalidate();
   invalidate();

   if ( strlen(szBuff) < 11 )
   {
      m_iConfirmationId = 1;
      MenuConfirmation* pMC = new MenuConfirmation("Invalid Pass Phrase","You need to specify a pass phrase that has at least 3 words each at least 3 characters long.",m_iConfirmationId, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return 1;
   }

   spp(szBuff);
   m_bHasPassPhrase = true;

   m_iConfirmationId = 1;
   MenuConfirmation* pMC = new MenuConfirmation("Pass Phrase Updated","Your pass phrase was updated. You need to enable encryption on the vehicles you wish (use [Vehicle]->[General] menu for that).",m_iConfirmationId, true);
   pMC->m_yPos = 0.3;

   char szInfo[256];
   snprintf(szInfo, sizeof(szInfo), "Your new pass phrase is: %s", szBuff);
   pMC->addTopLine(szInfo);
   add_menu_to_stack(pMC);

   log_line("Send local message about pp change");

   t_packet_header PH; 
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED;
   handle_commands_send_ruby_message(&PH, NULL, 0);
         
   return 1;
}


void MenuControllerEncryption::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 1 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      return;
   }
   if ( 2 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      m_pItemPass->setCurrentValue("");
      m_pItemPass->beginEdit();
      m_pItemPass->invalidate();
      invalidate();
      return;
   }
   m_iConfirmationId = 0;
}

void MenuControllerEncryption::onSelectItem()
{
   if ( 2 == m_SelectedIndex )
   {
      if ( ! m_pItemPass->isEditing() )
      {
         int countEncr = 0;
         for( int i=0; i<getModelsCount(); i++ )
         {
            Model* pModel = getModelAtIndex(i);
            if ( pModel->enc_flags != MODEL_ENC_FLAGS_NONE )
               countEncr++;
         }

         if ( m_bHasPassPhrase && countEncr > 0 )
         {
            m_iConfirmationId = 2;
            MenuConfirmation* pMC = new MenuConfirmation("Pass Phrase Change Confirmation","Do you want to change your pass phrase? You will no longer be able to communicate with the following vehicles that already use encryption and the current keys:",m_iConfirmationId);
            pMC->addTopLine("These vehicles are currently use encryption and the current keys:");
            pMC->addTopLine("");
            for( int i=0; i<getModelsCount(); i++ )
            {
               Model* pModel = getModelAtIndex(i);
               if ( pModel->enc_flags != MODEL_ENC_FLAGS_NONE )
               {
                  char szBuff[128];
                  snprintf(szBuff, sizeof(szBuff), "%s", pModel->getLongName());
                  pMC->addTopLine(szBuff);
               }
            }
            pMC->addTopLine("");
            pMC->addTopLine("You should first disable encryption on these vehicles before you change the pass phrase.");
            pMC->addTopLine("Are you sure you want to continue?");
            pMC->m_yPos = 0.24;
            add_menu_to_stack(pMC);
            return;
         }
         m_pItemPass->setCurrentValue("");
      }
      m_pItemPass->beginEdit();
      m_pItemPass->invalidate();
      invalidate();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }
}

