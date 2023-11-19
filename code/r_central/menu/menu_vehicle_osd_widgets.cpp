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

#include "menu.h"
#include "menu_vehicle_osd_widgets.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../osd/osd_widgets.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>


MenuVehicleOSDWidgets::MenuVehicleOSDWidgets(void)
:Menu(MENU_ID_VEHICLE_OSD_WIDGETS, "OSD Widgets", NULL)
{
   m_Width = 0.22;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;
}

MenuVehicleOSDWidgets::~MenuVehicleOSDWidgets()
{
}

void MenuVehicleOSDWidgets::valuesToUI()
{
}


bool MenuVehicleOSDWidgets::periodicLoop()
{
   return false;
}

void MenuVehicleOSDWidgets::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleOSDWidgets::stopAction()
{
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;
   //menu_setGlobalAlpha(m_fMenuOrgAlpha);
}

void MenuVehicleOSDWidgets::onMoveUp(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      return;
   }
   Menu::onMoveUp(bIgnoreReversion);
}

void MenuVehicleOSDWidgets::onMoveDown(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      return;
   }
   Menu::onMoveDown(bIgnoreReversion);
}

void MenuVehicleOSDWidgets::onMoveLeft(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      onMoveUp(bIgnoreReversion);
   }
   Menu::onMoveLeft(bIgnoreReversion);
}

void MenuVehicleOSDWidgets::onMoveRight(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      onMoveDown(bIgnoreReversion);
   }
   Menu::onMoveRight(bIgnoreReversion);
}


int MenuVehicleOSDWidgets::onBack()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      stopAction();
      return 1;
   }
   return 0;
}


void MenuVehicleOSDWidgets::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   m_iConfirmationId = 0;
}


void MenuVehicleOSDWidgets::onSelectItem()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      stopAction();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;
}
