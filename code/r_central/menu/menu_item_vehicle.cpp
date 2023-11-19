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

#include "menu_item_vehicle.h"
#include "menu_objects.h"
#include "menu.h"
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
   m_RenderTitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
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

   m_RenderTitleWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_pszTitle);

   m_RenderTitleWidth += getItemHeight(0.0);
   if ( m_bShowArrow )
      m_RenderTitleWidth += 2.0*g_pRenderEngine->textHeight(g_idFontMenu);
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
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   bool bIsCurrentVehicle = false;
   if ( (NULL != pModel) && (NULL != g_pCurrentModel) && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == pModel->vehicle_id )
      bIsCurrentVehicle = true;

   float fIconHeight = height_text;
   if ( bIsCurrentVehicle )
      fIconHeight = height_text*1.9;
   float fIconWidth = fIconHeight/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   if ( bIsCurrentVehicle )
      g_pRenderEngine->drawIcon(xPos, yPos + height_text*0.2, fIconWidth, fIconHeight, idIcon);
   else
      g_pRenderEngine->drawIcon(xPos, yPos + height_text*0.02, fIconWidth, fIconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bIsCurrentVehicle )
      RenderBaseTitle(xPos+fIconWidth + height_text*0.5, yPos + height_text*0.6, bSelected, fWidthSelection - fIconWidth - height_text*0.8);
   else
      RenderBaseTitle(xPos+fIconWidth + height_text*0.5, yPos, bSelected, fWidthSelection - fIconWidth - height_text*0.8);

   if ( (NULL != pModel) && (NULL != g_pCurrentModel) && (!g_pCurrentModel->is_spectator) && (g_pCurrentModel->vehicle_id == pModel->vehicle_id) )
   {
      float maxWidth = fWidthSelection + Menu::getSelectionPaddingX();
      float h2 = getItemHeight(0.0) + 0.5*Menu::getSelectionPaddingY();

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuText());
      g_pRenderEngine->setFill(250,250,250,0.1);   
      g_pRenderEngine->drawRoundRect(xPos-0.5*Menu::getSelectionPaddingX(), yPos, maxWidth, h2, 0.1*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
}


void MenuItemVehicle::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   MenuItem::RenderCondensed(xPos, yPos, bSelected, fWidthSelection);
}
