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
#include "../../base/models.h"
#include "../../base/config.h"
#include "../../base/commands.h"
#include "menu.h"
#include "menu_calibrate_hdmi.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_legend.h"

#include "../shared_vars.h"
#include "../pairing.h"
#include "../ruby_central.h"

static u32 s_idImageCalibrateHDMI = 0;

MenuCalibrateHDMI::MenuCalibrateHDMI(void)
:Menu(MENU_ID_CALIBRATE_HDMI, "Calibrate HDMI Output", NULL)
{
   m_Width = 0.7;
   m_Height = 0.76;
   m_xPos = 0.12;
   m_yPos = 0.14;
   if ( 0 == s_idImageCalibrateHDMI )
   {
      s_idImageCalibrateHDMI = g_pRenderEngine->loadImage("res/calibrate_hdmi.png");
      if ( 0 == s_idImageCalibrateHDMI )
         log_softerror_and_alarm("Failed to load tv mira image for HDMI calibration.");
      else
          log_line("Loaded image for TV mira for HDMI calibration.");
   }
   addTopLine("Adjust your controller output display brightness, contrast and saturation so that the colors and the shades of grey look as best as possible.");
   addTopLine("Press [Back]/[Cancel] when you are done.");
}

MenuCalibrateHDMI::~MenuCalibrateHDMI()
{
}

void MenuCalibrateHDMI::valuesToUI()
{
}


void MenuCalibrateHDMI::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);

   if ( 0 != s_idImageCalibrateHDMI )
      g_pRenderEngine->drawImage(m_RenderXPos+0.04, yTop+0.02, m_RenderWidth*0.8, m_RenderWidth*0.8, s_idImageCalibrateHDMI);
}

void MenuCalibrateHDMI::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

}