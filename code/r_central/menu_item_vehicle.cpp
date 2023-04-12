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
#include "menu_item_vehicle.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"
#include "osd_common.h"

MenuItemVehicle::MenuItemVehicle(const char* title)
:MenuItem(title)
{
   m_iVehicleIndex = -1;
}

MenuItemVehicle::MenuItemVehicle(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_iVehicleIndex = -1;
}

MenuItemVehicle::~MenuItemVehicle()
{
}

void MenuItemVehicle::setVehicleIndex(int vehicleIndex)
{
   m_iVehicleIndex = vehicleIndex;
}


void MenuItemVehicle::beginEdit()
{
   MenuItem::beginEdit();
}

void MenuItemVehicle::endEdit(bool bCanceled)
{
   MenuItem::endEdit(bCanceled);
}


float MenuItemVehicle::getItemHeight(float maxWidth)
{
   if ( m_RenderHeight > 0.001 )
      return m_RenderHeight;
   m_RenderTitleHeight = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   m_RenderHeight = m_RenderTitleHeight*1.2;
   if ( -1 != m_iVehicleIndex && NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) )
   {
      Model *pModel = getModelAtIndex(m_iVehicleIndex);
      if ( pModel->vehicle_id == g_pCurrentModel->vehicle_id )
      {
         m_RenderTitleHeight *= 2.0;
         m_RenderHeight = m_RenderTitleHeight*1.1;
      }
   }
   return m_RenderHeight;
}


float MenuItemVehicle::getTitleWidth(float maxWidth)
{
   if ( m_RenderTitleWidth > 0.001 )
      return m_RenderTitleWidth;

   float height_text = MENU_FONT_SIZE_ITEMS*menu_getScaleMenus();
   m_RenderTitleWidth = g_pRenderEngine->textWidth(height_text, g_idFontMenu, m_pszTitle);

   m_RenderTitleWidth += getItemHeight(0.0);
   if ( m_bShowArrow )
      m_RenderTitleWidth += 0.024*menu_getScaleMenus();
   return m_RenderTitleWidth;
}

float MenuItemVehicle::getValueWidth(float maxWidth)
{
   return 0.0;
}


void MenuItemVehicle::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( m_iVehicleIndex == -1 )
   {
      m_RenderLastY = yPos;
      m_RenderLastX = xPos;
      RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);
      return;
   }

   Model *pModel = getModelAtIndex(m_iVehicleIndex);
   u32 idIcon = osd_getVehicleIcon( pModel->vehicle_type );
   float height_text = MENU_FONT_SIZE_ITEMS*menu_getScaleMenus();

   bool bCurrent = false;
   if ( NULL != pModel && NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == pModel->vehicle_id )
      bCurrent = true;

   float fIconHeight = height_text;
   if ( bCurrent)
      fIconHeight = height_text*2.0;
   float fIconWidth = fIconHeight/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   if ( bCurrent )
      g_pRenderEngine->drawIcon(xPos, yPos + height_text*0.2, fIconWidth, fIconHeight, idIcon);
   else
      g_pRenderEngine->drawIcon(xPos, yPos + height_text*0.1, fIconWidth, fIconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bCurrent )
      RenderBaseTitle(xPos+fIconWidth + height_text*0.5, yPos+ height_text*0.6, bSelected, fWidthSelection - fIconWidth - height_text*0.5);
   else
      RenderBaseTitle(xPos+fIconWidth + height_text*0.5, yPos, bSelected, fWidthSelection - fIconWidth - height_text*0.5);

   if ( NULL != pModel && NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == pModel->vehicle_id )
   {
         float maxWidth = fWidthSelection + fIconWidth + height_text;
         float x = xPos+ 0.4*MENU_MARGINS*menu_getScaleMenus();
         float h2 = getItemHeight(0.0) + 0.3*MENU_MARGINS*menu_getScaleMenus();
   
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStroke(get_Color_MenuText());
         g_pRenderEngine->setFill(250,250,250,0.1);   
         g_pRenderEngine->drawRoundRect(xPos-height_text*0.3, yPos-0.07*MENU_MARGINS*menu_getScaleMenus(), maxWidth, h2, 0.005*menu_getScaleMenus());
         g_pRenderEngine->setColors(get_Color_MenuText());
   }
}


void MenuItemVehicle::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   MenuItem::RenderCondensed(xPos, yPos, bSelected, fWidthSelection);
}
