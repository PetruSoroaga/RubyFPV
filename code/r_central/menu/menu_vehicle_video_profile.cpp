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

#include "../../base/hardware_i2c.h"
#include "menu.h"
#include "menu_vehicle_video_profile.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"

MenuVehicleVideoProfileSelector::MenuVehicleVideoProfileSelector(void)
:Menu(MENU_ID_VEHICLE_VIDEO_PROFILE, "Select Video Profile", NULL)
{
   m_Width = 0.32;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;
   m_ExtraItemsHeight = 0;

   char szLegend[256];
   
   m_pItemsRadio[0] = new MenuItemRadio("", "");

   strcpy(szLegend, "Use this option to automatically adjust video parameters for video low latency;");
   m_pItemsRadio[0]->addSelection("High Performance", szLegend);

   strcpy(szLegend, "Use this option to automatically adjust video parameters for better video quality;");
   m_pItemsRadio[0]->addSelection("High Quality", szLegend);

   strcpy(szLegend, "Use this option if you wish to manually modify the advanced video parameters;");
   m_pItemsRadio[0]->addSelection("User Defined", szLegend);

   m_pItemsRadio[0]->setEnabled(true);
   m_IndexVideoProfile = addMenuItem(m_pItemsRadio[0]);

   addMenuItem(new MenuItemText("Note: Encodings and advanced video parameters can not be changed when in \"High Performance\" or \"High Quality\" mode") );
   addMenuItem(new MenuItemText("Encodings and advanced video parameters can be change only when in \"User\" mode, from the \"Advanced Video Parameters\" menu") );
}

MenuVehicleVideoProfileSelector::~MenuVehicleVideoProfileSelector()
{
}

void MenuVehicleVideoProfileSelector::onShow()
{      
   Menu::onShow();
}

void MenuVehicleVideoProfileSelector::valuesToUI()
{
   int index = 2;
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF )
      index = 0;
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY )
      index = 1;
   m_pItemsRadio[0]->setSelectedIndex(index);
   m_pItemsRadio[0]->setFocusedIndex(index);
}


void MenuVehicleVideoProfileSelector::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideoProfileSelector::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexVideoProfile == m_SelectedIndex )
   {
      log_line("Selected option %d", m_pItemsRadio[0]->getFocusedIndex());
      m_pItemsRadio[0]->setSelectedIndex(m_pItemsRadio[0]->getFocusedIndex());

      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      int index = m_pItemsRadio[0]->getSelectedIndex();
      g_pCurrentModel->video_params.user_selected_video_link_profile = VIDEO_PROFILE_USER;
      if ( index == 0 )
         g_pCurrentModel->video_params.user_selected_video_link_profile = VIDEO_PROFILE_BEST_PERF;
      if ( index == 1 )
         g_pCurrentModel->video_params.user_selected_video_link_profile = VIDEO_PROFILE_HIGH_QUALITY;

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      log_line("Sending to vehicle new user selected video link profile: %s", str_get_video_profile_name(paramsNew.user_selected_video_link_profile));
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      menu_stack_pop(1);
   }
}
