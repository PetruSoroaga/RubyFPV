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

#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/hw_procs.h"
#include "../local_stats.h"

#include "menu.h"
#include "menu_confirmation_delete_logs.h"
#include "../ruby_central.h"
#include "../events.h"

MenuConfirmationDeleteLogs::MenuConfirmationDeleteLogs(u32 uFreeSpaceMb, u32 uLogsSizeBytes)
:Menu(MENU_ID_CONFIRMATION_DELETE_LOGS, "Delete vehicle logs", NULL)
{
   m_iConfirmationId = 0;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;

   m_uFreeSpaceMb = uFreeSpaceMb;
   m_uLogSizeBytes = uLogsSizeBytes;

   char szBuff[256];
   sprintf(szBuff, "Vehicle is running out of free storage space. %u Mb free storage available.", m_uFreeSpaceMb);
   addTopLine(szBuff);
   
   if ( m_uLogSizeBytes >= 1000000 )
      sprintf(szBuff, "Vehicle logs take up %u Mb of storage.", m_uLogSizeBytes/1000/1000);
   else
      sprintf(szBuff, "Vehicle logs take up %u bytes of storage.", m_uLogSizeBytes);
   addTopLine(szBuff);

   addTopLine("Do you want to delete the logs?");
   addMenuItem(new MenuItem("No"));
   addMenuItem(new MenuItem("Yes"));
   m_SelectedIndex = 1;
}

MenuConfirmationDeleteLogs::~MenuConfirmationDeleteLogs()
{
   log_line("Closed MenuConfirmationDeleteLogs.");
}

void MenuConfirmationDeleteLogs::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 1;
}

int MenuConfirmationDeleteLogs::onBack()
{
   return Menu::onBack();
}

void MenuConfirmationDeleteLogs::onSelectItem()
{
   log_line("MenuConfirmationDeleteLogs: selected item: %d", m_SelectedIndex);

   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_CLEAR_LOGS, 1, NULL, 0); 
      menu_stack_pop(0);
   }
}