/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/hw_procs.h"
#include "../local_stats.h"

#include "menu.h"
#include "menu_confirmation_hdmi.h"
#include "../ruby_central.h"
#include "../events.h"

MenuConfirmationHDMI::MenuConfirmationHDMI(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION_HDMI, szTitle, NULL)
{
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
   sprintf(szBuff, "rm -rf %s%s", FOLDER_CONFIG, FILE_TEMP_HDMI_CHANGED);
   hw_execute_bash_command(szBuff, NULL);
}

void MenuConfirmationHDMI::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 1;
}

void MenuConfirmationHDMI::onSelectItem()
{
   log_line("Menu Confirmation HDMI: selected item: %d", m_SelectedIndex);

   if ( 0 == m_SelectedIndex )
   {
      log_line("Reverting HDMI resolution change, user confirmed it...");
      onEventReboot();

      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_TEMP_HDMI_CHANGED);
      FILE* fd = fopen(szFile, "r");
      if ( NULL != fd )
      {
         char szBuff[128];
         int group, mode;
         int tmp1, tmp2, tmp3;
         fscanf(fd, "%d %d", &tmp1, &tmp2 );
         fscanf(fd, "%d %d %d", &tmp1, &tmp2, &tmp3 );
         fscanf(fd, "%d %d", &group, &mode );
         fclose(fd);
         log_line("Reverting HDMI resolution back to: group: %d, mode: %d", group, mode);
         sprintf(szBuff, "rm -rf %s%s", FOLDER_CONFIG, FILE_TEMP_HDMI_CHANGED);
         hw_execute_bash_command(szBuff, NULL);

         hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

         sprintf(szBuff, "sed -i 's/hdmi_group=[0-9]*/hdmi_group=%d/g' config.txt", group);
         hw_execute_bash_command(szBuff, NULL);
         sprintf(szBuff, "sed -i 's/hdmi_mode=[0-9]*/hdmi_mode=%d/g' config.txt", mode);
         hw_execute_bash_command(szBuff, NULL);
         hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      }

      menu_stack_pop(0);
      hardware_reboot();
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      log_line("Confirmed HDMI resolution change.");
      menu_stack_pop(1);
   }
}