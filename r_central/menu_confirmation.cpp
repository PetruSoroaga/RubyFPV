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
#include "menu.h"
#include "menu_confirmation.h"

MenuConfirmation::MenuConfirmation(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION, szTitle, NULL)
{
   m_iConfirmationId = id;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bSingleOption = false;
   addTopLine(szText);
   addMenuItem(new MenuItem("No"));
   addMenuItem(new MenuItem("Yes"));
}

MenuConfirmation::MenuConfirmation(const char* szTitle, const char* szText, int id, bool singleOption)
:Menu(MENU_ID_CONFIRMATION, szTitle, NULL)
{
   m_iConfirmationId = id;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bSingleOption = singleOption;
   addTopLine(szText);
   if ( m_bSingleOption )
   {
      addMenuItem(new MenuItem("Ok"));
      m_yPos = 0.45;
   }
   else
   {
      addMenuItem(new MenuItem("No"));
      addMenuItem(new MenuItem("Yes"));
   }
}

MenuConfirmation::~MenuConfirmation()
{
}


void MenuConfirmation::onSelectItem()
{
   log_line("Menu Confirmation: selected item: %d", m_SelectedIndex);
   if ( m_bSingleOption )
   {
      menu_stack_pop(1);
      return;
   }

   if ( 0 == m_SelectedIndex )
      menu_stack_pop(0);

   if ( 1 == m_SelectedIndex )
      menu_stack_pop(1);
}