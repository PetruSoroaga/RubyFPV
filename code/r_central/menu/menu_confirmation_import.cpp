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
#include "../../base/ctrl_interfaces.h"
#include "../../base/ctrl_settings.h"
#include "../../base/hardware.h"
#include "../../base/hw_procs.h"
#include "../../base/controller_utils.h"
#include "../local_stats.h"

#include "menu.h"
#include "menu_confirmation_import.h"
#include "../ruby_central.h"
#include "../events.h"
#include "../pairing.h"


MenuConfirmationImport::MenuConfirmationImport(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION, szTitle, NULL)
{
   m_iConfirmationId = id;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   addTopLine(szText);
   addMenuItem(new MenuItem("No"));
   addMenuItem(new MenuItem("Yes"));
   m_SelectedIndex = 0;
}

MenuConfirmationImport::~MenuConfirmationImport()
{
}

void MenuConfirmationImport::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 0;
}

int MenuConfirmationImport::onBack()
{
   return Menu::onBack();
}

void MenuConfirmationImport::onSelectItem()
{
   log_line("Menu Confirmation Import: selected item: %d", m_SelectedIndex);

   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(1);
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      int nResult = -1;
      pairing_stop();
      if ( controller_utils_usb_import_has_matching_controller_id_file() )
         nResult = controller_utils_import_all_from_usb(false);
      else if ( controller_utils_usb_import_has_any_controller_id_file() )
         nResult = controller_utils_import_all_from_usb(true);
      if ( nResult == 0 )
      {
         ruby_load_models();

         if ( ! load_Preferences() )
            save_Preferences();

         if ( ! load_ControllerSettings() )
            save_ControllerSettings();

         if ( ! load_ControllerInterfacesSettings() )
            save_ControllerInterfacesSettings();
   
         hw_execute_bash_command("sudo reboot -f", NULL);
      }
      else
         addMessage("Failed to import settings from USB memory stick.");         
      return;
   }
}