/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
         * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
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
#include "menu_tx_power_8812eu.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_tx_power_max.h"

MenuTXPower8812EU::MenuTXPower8812EU()
:MenuTXPower()
{
   m_MenuId = MENU_ID_TX_POWER_8812EU;
   setTitle("Radio Output Power Levels (for RTL8812EU)");
   m_Width = 0.72;
   m_Height = 0.61;
   m_xPos = 0.05;
   m_yPos = 0.16;

   m_xTable = m_RenderXPos + m_sfMenuPaddingY;
   m_xTable += 0.15*m_sfScaleFactor;
   m_xTableCellWidth = 0.05*m_sfScaleFactor;

   m_bShowThinLine = false;
   
   m_bShowVehicle = true;
   m_bShowController = true;
   
   m_bValuesChangedVehicle = false;
   m_bValuesChangedController = false;
}

MenuTXPower8812EU::~MenuTXPower8812EU()
{
}

void MenuTXPower8812EU::onShow()
{
   
} 
      
void MenuTXPower8812EU::valuesToUI()
{
}


void MenuTXPower8812EU::Render()
{
}

int MenuTXPower8812EU::onBack()
{
   return Menu::onBack(); 
}

void MenuTXPower8812EU::onReturnFromChild(int iChildMenuId, int returnValue)
{
   
}

void MenuTXPower8812EU::onSelectItem()
{
   
}