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
#include "../osd/osd_common.h"
#include "menu_vehicle_general.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"

#include "../link_watch.h"
#include "../launchers_controller.h"
#include "../../base/encr.h"

MenuVehicleGeneral::MenuVehicleGeneral(void)
:Menu(MENU_ID_VEHICLE_GENERAL, "General Vehicle Settings", NULL)
{
   m_Width = 0.26;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.21;
   valuesToUI();
}

MenuVehicleGeneral::~MenuVehicleGeneral()
{
}

void MenuVehicleGeneral::addTopDescription()
{
   Preferences* pP = get_Preferences();
   removeAllTopLines();
   char szBuff[256];
   char szType[32];

   strcpy(szType, "runs");
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      strcpy(szType, "flights");
   
   sprintf(szBuff, "Total %s: %d", szType, g_pCurrentModel->m_Stats.uTotalFlights);
   addTopLine(szBuff);

   int sec = (g_pCurrentModel->m_Stats.uTotalOnTime)%60;
   int min = (g_pCurrentModel->m_Stats.uTotalOnTime/60)%60;
   int hours = (g_pCurrentModel->m_Stats.uTotalOnTime/3600);

   sprintf(szBuff, "Total ON time: %dh:%02dm:%02ds", hours, min, sec);
   addTopLine(szBuff);

   sec = (g_pCurrentModel->m_Stats.uTotalFlightTime)%60;
   min = (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60;
   hours = (g_pCurrentModel->m_Stats.uTotalFlightTime/3600);

   strcpy(szType, "run time");
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      strcpy(szType, "flight time");
   sprintf(szBuff, "Total %s: %dh:%02dm:%02ds", szType, hours, min, sec);
   addTopLine(szBuff);

   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "Odometer: %.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   else
      sprintf(szBuff, "Odometer: %.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   addTopLine(szBuff);

   char szBuff2[64];
   getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);

   sprintf(szBuff, "SW Version: %s", szBuff2);
   addTopLine(szBuff);
   
   addTopLine("");

   m_ExtraItemsHeight = 0;
   return;
}

void MenuVehicleGeneral::populate()
{
   removeAllItems();
   addTopDescription();

   char szBuff[128];

   m_pItemEditName = new MenuItemEdit("Name", g_pCurrentModel->vehicle_name);
   m_pItemEditName->setMaxLength(MAX_VEHICLE_NAME_LENGTH-1);
   addMenuItem(m_pItemEditName);

   m_pItemsSelect[0] = new MenuItemSelect("Vehicle Type", "Changes the vehicle type Ruby is using. Has impact on things like OSD elements, telemetry parsing.");
   m_pItemsSelect[0]->addSelection("Generic");
   m_pItemsSelect[0]->addSelection("Drone");
   m_pItemsSelect[0]->addSelection("Airplane");
   m_pItemsSelect[0]->addSelection("Helicopter");
   m_pItemsSelect[0]->addSelection("Car");
   m_pItemsSelect[0]->addSelection("Boat");
   m_pItemsSelect[0]->addSelection("Robot");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexVehicleType = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Has GPS", "Sets how many GPS units are configured on the vehicle. Ruby uses GPS data for OSD display.");  
   m_pItemsSelect[1]->addSelection("No GPS");
   m_pItemsSelect[1]->addSelection("1 GPS unit");
   m_pItemsSelect[1]->addSelection("2 GPS units");
   m_pItemsSelect[1]->addSelection("3 GPS units");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexGPS = addMenuItem(m_pItemsSelect[1]);
}

void MenuVehicleGeneral::valuesToUI()
{
   populate();

   m_pItemsSelect[0]->setSelection(g_pCurrentModel->vehicle_type);
   m_pItemsSelect[1]->setSelection(g_pCurrentModel->iGPSCount);
}

void MenuVehicleGeneral::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 4.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   g_pRenderEngine->drawIcon(m_RenderXPos+m_RenderWidth - iconWidth - m_sfMenuPaddingX , m_RenderYPos + m_RenderHeaderHeight+m_sfMenuPaddingY + iconHeight*0.05, iconWidth, iconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuVehicleGeneral::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

int MenuVehicleGeneral::onBack()
{
   if ( 0 == m_SelectedIndex )
   if ( m_pMenuItems[0]->isEditing() )
   {
      m_pMenuItems[0]->endEdit(false);
      char szBuff[MAX_VEHICLE_NAME_LENGTH];
      strcpy(szBuff, m_pItemEditName->getCurrentValue());
      str_sanitize_modelname(szBuff);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_NAME, 0, (u8*)szBuff, MAX_VEHICLE_NAME_LENGTH) )
         m_pItemEditName->setCurrentValue(g_pCurrentModel->vehicle_name);
    
      return 1;
   }

   return Menu::onBack();
}

void MenuVehicleGeneral::onSelectItem()
{
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( 0 == m_SelectedIndex )
   {
      m_pMenuItems[0]->beginEdit();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexVehicleType == m_SelectedIndex )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_TYPE, (u8)(m_pItemsSelect[0]->getSelectedIndex()), NULL, 0) )
         m_pItemsSelect[0]->setSelection(g_pCurrentModel->vehicle_type);
   }

   if ( m_IndexGPS == m_SelectedIndex )
   {
      int iGPSCount = m_pItemsSelect[1]->getSelectedIndex();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_GPS_INFO, iGPSCount, NULL, 0) )
         valuesToUI();
   }
}