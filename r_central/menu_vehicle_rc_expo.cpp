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
#include "../base/utils.h"
#include "menu.h"
#include "menu_vehicle_rc_expo.h"
#include "menu_confirmation.h"

#include "shared_vars.h"
#include "pairing.h"
#include "colors.h"
#include "ruby_central.h"

MenuVehicleRCExpo::MenuVehicleRCExpo(void)
:Menu(MENU_ID_VEHICLE_RC_EXPO, "Channels Expo", NULL)
{
   m_Width = 0.25;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;
   m_ExtraHeight = 0;//2*toScreenX(MENU_MARGINS)*menu_getScaleMenus();
   setColumnsCount(1);
   float fSliderWidth = 0.10;
   char szBuff[128];

   m_ChannelCount = g_pCurrentModel->rc_params.channelsCount; 
   for( int i=0; i<m_ChannelCount; i++ )
   {
      g_pCurrentModel->get_rc_channel_name(i, szBuff);
      //m_ItemsChannels[i].pItemTitle = new MenuItem(szBuff);
      //m_ItemsChannels[i].m_IndexTitle = addMenuItem(m_ItemsChannels[i].pItemTitle);

      m_ItemsChannels[i].pItemValue = new MenuItemRange(szBuff, "Set an exponential curve value for this RC channel.", 0, 80, 0, 1 );
      m_ItemsChannels[i].pItemValue->setSufix("%");
      m_ItemsChannels[i].m_IndexValue = addMenuItem(m_ItemsChannels[i].pItemValue);
   }
   m_IndexComputedChannel = 0;
   computeDisplayCurve(0);
}

MenuVehicleRCExpo::~MenuVehicleRCExpo()
{
}

void MenuVehicleRCExpo::onShow()
{      
   Menu::onShow();
   int index = m_SelectedIndex/m_iColumnsCount;  
   computeDisplayCurve(index);
}

void MenuVehicleRCExpo::valuesToUI()
{
   for( int i=0; i<m_ChannelCount; i++ )
      m_ItemsChannels[i].pItemValue->setCurrentValue(g_pCurrentModel->rc_params.rcChExpo[i]);

   int index = m_SelectedIndex/m_iColumnsCount;  
   computeDisplayCurve(index);
}

void MenuVehicleRCExpo::computeDisplayCurve(int nChannel)
{
   m_IndexComputedChannel = nChannel;

   for( int i=0; i<DISPLAY_CURVE_VALUES; i++ )
   {
      float fPercent = (float)i/(float)(DISPLAY_CURVE_VALUES-1);
      //m_fDisplayCurveValues[i] = 1000 + fPercent*1000.0;
      m_fDisplayCurveValues[i] = compute_output_rc_value(g_pCurrentModel, m_IndexComputedChannel, 0.01, fPercent, 2);
   }
}

void MenuVehicleRCExpo::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   int index = m_SelectedIndex/m_iColumnsCount;  
   if ( index != m_IndexComputedChannel )
      computeDisplayCurve(index);

   RenderEnd(yTop);

   float fWidthValue = g_pRenderEngine->textWidth(0, g_idFontMenu, "000%");
   float height_text_small = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenuSmall);
   float fHeight = 0.15 * menu_getScaleMenus();
   float fWidth = fHeight / g_pRenderEngine->getAspectRatio();
   float xPos = m_RenderXPos + m_RenderWidth - fWidthValue - (0.02+MENU_MARGINS)*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() - fWidth;
   float yPos = yTop + 0.02 * menu_getScaleMenus();

   float fAlfa = g_pRenderEngine->setGlobalAlfa(0.5);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);
   g_pRenderEngine->drawLine(xPos, yPos, xPos+fWidth, yPos);
   g_pRenderEngine->drawLine(xPos, yPos+fHeight, xPos+fWidth, yPos+fHeight);
   g_pRenderEngine->drawLine(xPos, yPos, xPos, yPos+fHeight);
   g_pRenderEngine->drawLine(xPos+fWidth, yPos, xPos+fWidth, yPos+fHeight);

   for( float x = xPos; x<=xPos+fWidth-0.01/g_pRenderEngine->getAspectRatio(); x += 0.01/g_pRenderEngine->getAspectRatio() )
      g_pRenderEngine->drawLine(x, yPos+fHeight*0.5, x + 0.005/g_pRenderEngine->getAspectRatio(), yPos+fHeight*0.5);

   for( float y = yPos; y<=yPos+fHeight-0.01; y += 0.01)
      g_pRenderEngine->drawLine(xPos + fWidth*0.5, y, xPos + fWidth*0.5, y + 0.005);
   
   g_pRenderEngine->setGlobalAlfa(fAlfa);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);

   g_pRenderEngine->drawText(xPos, yPos+fHeight, height_text_small, g_idFontMenuSmall, "0%");
   g_pRenderEngine->drawTextLeft(xPos+fWidth, yPos+fHeight, height_text_small, g_idFontMenuSmall, "100%");

   char szBuff[32];
   sprintf(szBuff, "%d", g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel]);
   g_pRenderEngine->drawTextLeft(xPos-0.005*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yPos, height_text_small, g_idFontMenuSmall, szBuff);
   sprintf(szBuff, "%d", g_pCurrentModel->rc_params.rcChMid[m_IndexComputedChannel]);
   g_pRenderEngine->drawTextLeft(xPos-0.005*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yPos+fHeight*0.5 - height_text_small*0.5, height_text_small, g_idFontMenuSmall, szBuff);
   sprintf(szBuff, "%d", g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]);
   g_pRenderEngine->drawTextLeft(xPos-0.005*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yPos+fHeight-height_text_small, height_text_small, g_idFontMenuSmall, szBuff);

   if ( g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel] < g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]+0.001 )
      return;

   for( int i=0; i<DISPLAY_CURVE_VALUES-1; i++ )
   {
      float percent1 = ((float)i)/(float)(DISPLAY_CURVE_VALUES-1);
      float percent2 = ((float)i+1)/(float)(DISPLAY_CURVE_VALUES-1);
      float x1 = xPos + fWidth*percent1;
      float x2 = xPos + fWidth*percent2;
      float y1 = yPos + fHeight - fHeight * (m_fDisplayCurveValues[i]-g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel])/((g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel]-g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]));
      float y2 = yPos + fHeight - fHeight * (m_fDisplayCurveValues[i+1]-g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel])/((g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel]-g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]));
      g_pRenderEngine->drawLine(x1,y1,x2,y2);
   }
}

void MenuVehicleRCExpo::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   int fsvalue = m_ItemsChannels[m_IndexComputedChannel].pItemValue->getCurrentValue();
   g_pCurrentModel->rc_params.rcChExpo[m_IndexComputedChannel] = fsvalue;

   computeDisplayCurve(m_IndexComputedChannel);
   memcpy(&g_pCurrentModel->rc_params, &params, sizeof(rc_parameters_t));
}

void MenuVehicleRCExpo::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicleRCExpo::onSelectItem()
{
   int subIndex = m_SelectedIndex % m_iColumnsCount;

   //if ( subIndex == 0 )
   //{
   //   Menu::onSelectItem();
   //   return;
   //}

   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   int index = m_SelectedIndex/m_iColumnsCount;
   int fsvalue = m_ItemsChannels[index].pItemValue->getCurrentValue();

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   params.rcChExpo[index] = fsvalue;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();  
}