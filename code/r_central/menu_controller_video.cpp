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
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_video.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_calibrate_hdmi.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hdmi.h"

#include "../renderer/render_engine.h"

#include "pairing.h"
#include "colors.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"

#include <time.h>
#include <sys/resource.h>

const char* s_szHDMIInfo = "Note: Changing the HDMI options requires a restart of the controller for the changes to take effect.";

MenuControllerVideo::MenuControllerVideo(void)
:Menu(MENU_ID_CONTROLLER_VIDEO, "Video Output Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.25;
   
   Preferences* pp = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();
   char szBuff[64];

   m_hdmigroupOrg = hdmi_get_current_resolution_group();
   m_hdmimodeOrg = hdmi_get_current_resolution_mode();
   log_line("Current HDMI resolution: group: %d, mode: %d", m_hdmigroupOrg, m_hdmimodeOrg);
   addMenuItem(new MenuItemSection("HDMI Output"));

   m_pItemsSelect[0] = new MenuItemSelect("HDMI output resolution", "Sets the HDMI resolution on the controller display.");
   for( int i=0; i<hdmi_get_resolutions_count(); i++ )
   {
      sprintf(szBuff, "%d x %d", hdmi_get_resolution_width(i), hdmi_get_resolution_height(i));
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   m_pItemsSelect[0]->setIsEditable();
   m_IndexHDMIRes = addMenuItem(m_pItemsSelect[0]);
   
   m_pItemsSelect[3] = new MenuItemSelect("HDMI refresh rate", "Sets the HDMI refresh rate for the display.");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexHDMIRefreshRate = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[0] = new MenuItemSlider("HDMI Output Boost", "Sets the boost voltage level for the HDMI output.", 0,11,6, 0.15);
   m_IndexHDMIBoost = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[2] = new MenuItemSelect("HDMI Overscan", "Default overscan (margins) on HDMI output. Requires a reboot after changing this value.");
   m_pItemsSelect[2]->addSelection("Enabled");
   m_pItemsSelect[2]->addSelection("Disabled");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexHDMIOverscan = addMenuItem(m_pItemsSelect[2]);

   m_IndexCalibrateHDMI = addMenuItem(new MenuItem("Calibrate HDMI output", "Calibrate the colors, brightness and contrast on the controller display."));
   m_pMenuItems[m_IndexCalibrateHDMI]->showArrow();

   addMenuItem(new MenuItemSection("Other Video Outputs"));

   m_pItemsSelect[1] = new MenuItemSelect("Video Forward To USB Device", "Enables or disables forwarding of the video stream to an external device using a USB connection.");
   m_pItemsSelect[1]->addSelection("Disabled");
   m_pItemsSelect[1]->addSelection("Raw (H264)");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexVideoUSBForward = addMenuItem(m_pItemsSelect[1]);

   m_pItemsRange[0] = new MenuItemRange("    USB Port Number", "Sets the USB port number to forward video to.", 1025, 32000, 5001, 1 );
   m_pItemsRange[0]->setSufix("");
   m_IndexVideoUSBPort = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("    USB Packet Size", "Sets the data packet size to be send to USB. Some VR apps are sensitive to this value.", 10, 2048, 1024, 1 );
   m_pItemsRange[1]->setSufix("");
   m_IndexVideoUSBPacket = addMenuItem(m_pItemsRange[1]);


   m_pItemsSelect[10] = new MenuItemSelect("Video Forward To Network", "Enables or disables forwarding of the video stream to the local network using the ETH connection.");
   m_pItemsSelect[10]->addSelection("Disabled");
   m_pItemsSelect[10]->addSelection("Raw (H264)");
   m_pItemsSelect[10]->addSelection("RTP Stream");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexVideoETHForward = addMenuItem(m_pItemsSelect[10]);

   m_pItemsRange[10] = new MenuItemRange("    Network Port Number", "Sets the ETH port number to forward video to.", 1025, 32000, 5001, 1 );
   m_pItemsRange[10]->setSufix("");
   m_IndexVideoETHPort = addMenuItem(m_pItemsRange[10]);

   m_pItemsRange[11] = new MenuItemRange("    Network Packet Size", "Sets the data packet size to be sent to ETH.", 10, 2048, 1024, 1 );
   m_pItemsRange[11]->setSufix("");
   m_IndexVideoETHPacket = addMenuItem(m_pItemsRange[11]);

   m_ExtraHeight = g_pRenderEngine->getMessageHeight(s_szHDMIInfo, MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus(), MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   m_ExtraHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
}

MenuControllerVideo::~MenuControllerVideo(void)
{
   if ( m_hdmigroupOrg != hdmi_get_current_resolution_group() ||
        m_hdmimodeOrg != hdmi_get_current_resolution_mode() )
   {
      char szBuff[128];
      sprintf(szBuff, "touch %s", FILE_TMP_HDMI_CHANGED);
      hw_execute_bash_command_silent(szBuff, NULL);

      FILE* fd = fopen(FILE_TMP_HDMI_CHANGED, "w");
      if ( NULL != fd )
      {
         fprintf(fd, "%d %d\n", hdmi_get_current_resolution_group(), hdmi_get_current_resolution_mode() );
         fprintf(fd, "%d %d %d\n", hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );
         fprintf(fd, "%d %d\n", m_hdmigroupOrg, m_hdmimodeOrg );
         fclose(fd);
      }
   }
}

void MenuControllerVideo::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pp = get_Preferences();
   char szBuff[64];

   m_pItemsSlider[0]->setCurrentValue(pCS->iHDMIBoost);

   m_pItemsSelect[0]->setSelection(hdmi_get_current_resolution_index());

   m_pItemsSelect[3]->removeAllSelections();
   for( int i=0; i<hdmi_get_current_resolution_refresh_count(); i++ )
   {
      sprintf(szBuff, "%d Hz", hdmi_get_resolution_refresh_rate(hdmi_get_current_resolution_index(), i));
      m_pItemsSelect[3]->addSelection(szBuff);
   }
   m_pItemsSelect[3]->setSelection(hdmi_get_current_resolution_refresh_index());

   pCS->iDisableHDMIOverscan = config_file_get_value("disable_overscan");
   m_pItemsSelect[2]->setSelectedIndex(pCS->iDisableHDMIOverscan);

   m_pItemsSelect[1]->setSelectedIndex(pCS->iVideoForwardUSBType);
   m_pItemsRange[0]->setCurrentValue(pCS->iVideoForwardUSBPort);
   m_pItemsRange[1]->setCurrentValue(pCS->iVideoForwardUSBPacketSize);
   if ( 0 == pCS->iVideoForwardUSBType )
   {
      m_pItemsRange[0]->setEnabled(false);
      m_pItemsRange[1]->setEnabled(false);
   }
   else
   {
      m_pItemsRange[0]->setEnabled(true);
      m_pItemsRange[1]->setEnabled(true);
   }

   m_pItemsSelect[10]->setSelectedIndex(pCS->nVideoForwardETHType);
   m_pItemsRange[10]->setCurrentValue(pCS->nVideoForwardETHPort);
   m_pItemsRange[11]->setCurrentValue(pCS->nVideoForwardETHPacketSize);
   if ( 0 == pCS->nVideoForwardETHType )
   {
      m_pItemsRange[10]->setEnabled(false);
      m_pItemsRange[11]->setEnabled(false);
   }
   else
   {
      m_pItemsRange[10]->setEnabled(true);
      m_pItemsRange[11]->setEnabled(true);
   }
}

void MenuControllerVideo::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
      if ( i == m_IndexHDMIBoost )
      {
         y += 0.3 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         g_pRenderEngine->setColors(get_Color_MenuText());         
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + 0.01*menu_getScaleMenus(), y, s_szHDMIInfo, MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus(), MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
         y += 0.7 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
      }
   }
   RenderEnd(yTop);
}

void MenuControllerVideo::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
   m_iConfirmationId = 0;
}

void MenuControllerVideo::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( m_IndexHDMIBoost == m_SelectedIndex )
   {
      char szBuff[64];
      int hdmi_boost = m_pItemsSlider[0]->getCurrentValue();
      if ( hdmi_boost == pCS->iHDMIBoost )
         return;
      pCS->iHDMIBoost = hdmi_boost;

      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);
      sprintf(szBuff, "sed -i 's/config_hdmi_boost=[0-9]*/config_hdmi_boost=%d/g' config.txt", hdmi_boost);
      hw_execute_bash_command(szBuff, NULL);
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      
      save_ControllerSettings();
   }

   if ( m_IndexHDMIRes == m_SelectedIndex )
   {
      char szBuff[128];
      int index = m_pItemsSelect[0]->getSelectedIndex();
      hdmi_set_current_resolution(hdmi_get_resolution_width(index), hdmi_get_resolution_height(index), hdmi_get_resolution_refresh_rate(index,0));
      valuesToUI();
   }

   if ( m_IndexHDMIRefreshRate == m_SelectedIndex )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      int indexR = m_pItemsSelect[3]->getSelectedIndex();
      hdmi_set_current_resolution(hdmi_get_resolution_width(index), hdmi_get_resolution_height(index), hdmi_get_resolution_refresh_rate(index,indexR));
   }

   if ( m_IndexHDMIOverscan == m_SelectedIndex )
   {
      pCS->iDisableHDMIOverscan = m_pItemsSelect[2]->getSelectedIndex();
      save_ControllerSettings();
      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

      config_file_force_value("config.txt", "disable_overscan", pCS->iDisableHDMIOverscan);
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);

      invalidate();
      valuesToUI();
   }

   if ( m_IndexCalibrateHDMI == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuCalibrateHDMI());      
      return;
   }

   if ( m_IndexVideoUSBForward == m_SelectedIndex )
   {
      pCS->iVideoForwardUSBType = m_pItemsSelect[1]->getSelectedIndex();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
   if ( m_IndexVideoUSBPort == m_SelectedIndex )
   {
      pCS->iVideoForwardUSBPort = m_pItemsRange[0]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
   if ( m_IndexVideoUSBPacket == m_SelectedIndex )
   {
      pCS->iVideoForwardUSBPacketSize = m_pItemsRange[1]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }


   if ( m_IndexVideoETHForward == m_SelectedIndex )
   {
      pCS->nVideoForwardETHType = m_pItemsSelect[10]->getSelectedIndex();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
   if ( m_IndexVideoETHPort == m_SelectedIndex )
   {
      pCS->nVideoForwardETHPort = m_pItemsRange[10]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
   if ( m_IndexVideoETHPacket == m_SelectedIndex )
   {
      pCS->nVideoForwardETHPacketSize = m_pItemsRange[11]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
}
