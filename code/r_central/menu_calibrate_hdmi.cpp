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
#include "../base/models.h"
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "menu.h"
#include "menu_calibrate_hdmi.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_legend.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"

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