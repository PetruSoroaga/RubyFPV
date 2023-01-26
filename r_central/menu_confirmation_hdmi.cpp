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
#include "local_stats.h"

#include "menu.h"
#include "menu_confirmation_hdmi.h"
#include "ruby_central.h"
#include "events.h"

MenuConfirmationHDMI::MenuConfirmationHDMI(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION_HDMI, szTitle, NULL)
{
   m_iConfirmationId = id;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   addTopLine(szText);
   addTopLine("(If you don't select anything, it will auto revert to old HDMI configuration after a timeout)");
   addMenuItem(new MenuItem("No"));
   addMenuItem(new MenuItem("Yes"));
   m_SelectedIndex = 1;
}

MenuConfirmationHDMI::~MenuConfirmationHDMI()
{
   log_line("Closed HDMI resolution change confirmation dialog.");
   char szBuff[128];
   sprintf(szBuff, "rm -rf %s", FILE_TMP_HDMI_CHANGED);
   hw_execute_bash_command(szBuff, NULL);
}

void MenuConfirmationHDMI::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 1;
}

int MenuConfirmationHDMI::onBack()
{
   return Menu::onBack();
}

void MenuConfirmationHDMI::onSelectItem()
{
   log_line("Menu Confirmation HDMI: selected item: %d", m_SelectedIndex);

   if ( 0 == m_SelectedIndex )
   {
      log_line("Reverting HDMI resolution change, user confirmed it...");
      onEventReboot();

      FILE* fd = fopen(FILE_TMP_HDMI_CHANGED, "r");
      if ( NULL != fd )
      {
         char szBuff[256];
         int group, mode;
         int tmp1, tmp2, tmp3;
         fscanf(fd, "%d %d", &tmp1, &tmp2 );
         fscanf(fd, "%d %d %d", &tmp1, &tmp2, &tmp3 );
         fscanf(fd, "%d %d", &group, &mode );
         fclose(fd);
         log_line("Reverting HDMI resolution back to: group: %d, mode: %d", group, mode);
         sprintf(szBuff, "rm -rf %s", FILE_TMP_HDMI_CHANGED);
         hw_execute_bash_command(szBuff, NULL);

         hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

         sprintf(szBuff, "sed -i 's/hdmi_group=[0-9]*/hdmi_group=%d/g' config.txt", group);
         hw_execute_bash_command(szBuff, NULL);
         sprintf(szBuff, "sed -i 's/hdmi_mode=[0-9]*/hdmi_mode=%d/g' config.txt", mode);
         hw_execute_bash_command(szBuff, NULL);
         hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      }

      menu_stack_pop(0);
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      log_line("Confirmed HDMI resolution change.");
      menu_stack_pop(1);
   }
}