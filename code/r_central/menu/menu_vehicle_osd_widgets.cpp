/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
   return Menu::onBack();
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
