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
#include "menu_vehicle_rc_failsafe.h"
#include "menu_confirmation.h"

MenuVehicleRCFailsafe::MenuVehicleRCFailsafe(void)
:Menu(MENU_ID_VEHICLE_RC_FAILSAFE, "Channels Failsafe Values", NULL)
{
   m_Width = 0.34;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;
   m_ExtraItemsHeight = 0;
   setColumnsCount(3);
   float fSliderWidth = 0.10;
   char szBuff[128];

   m_ChannelCount = g_pCurrentModel->rc_params.channelsCount; 
   for( int i=0; i<m_ChannelCount; i++ )
   {
      g_pCurrentModel->get_rc_channel_name(i, szBuff);

      m_ItemsChannels[i].pItemTitle = new MenuItem(szBuff);
      m_ItemsChannels[i].m_IndexTitle = addMenuItem(m_ItemsChannels[i].pItemTitle);

      m_ItemsChannels[i].pItemType = new MenuItemSelect("Failsafe Type", "Sets what output will be generated for this channel when a failsafe event occurs.");
      m_ItemsChannels[i].pItemType->addSelection("Keep last");
      m_ItemsChannels[i].pItemType->addSelection("Below range");
      m_ItemsChannels[i].pItemType->addSelection("Value");
      m_ItemsChannels[i].pItemType->setIsEditable();
      m_ItemsChannels[i].pItemType->setCondensedOnly();
      m_ItemsChannels[i].m_IndexType = addMenuItem(m_ItemsChannels[i].pItemType);

      m_ItemsChannels[i].pItemValue = new MenuItemRange("Value:", "Set a fixed failsafe value for this RC channel.", 500, 2500, 1000, 1 );  
      m_ItemsChannels[i].pItemValue->setCondensedOnly();
      m_ItemsChannels[i].m_IndexValue = addMenuItem(m_ItemsChannels[i].pItemValue);
   }
}

MenuVehicleRCFailsafe::~MenuVehicleRCFailsafe()
{
}

void MenuVehicleRCFailsafe::onShow()
{      
   Menu::onShow();
}

void MenuVehicleRCFailsafe::valuesToUI()
{
   for( int i=0; i<m_ChannelCount; i++ )
   {
      m_ItemsChannels[i].pItemType->setSelection( ((g_pCurrentModel->rc_params.rcChFlags[i] >> 1) & 0x07) - 1 );
      m_ItemsChannels[i].pItemValue->setCurrentValue(g_pCurrentModel->rc_params.rcChFailSafe[i]);

     if ( m_ItemsChannels[i].pItemType->getSelectedIndex() == 2 )
        m_ItemsChannels[i].pItemValue->setEnabled(true);
     else
        m_ItemsChannels[i].pItemValue->setEnabled(false);
   }
}


void MenuVehicleRCFailsafe::Render()
{
   RenderPrepare();
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   float fFirstItemWidth = 0.08*Menu::getScaleFactor();
   float fSecItemWidth = 0.1*Menu::getScaleFactor();
   float fSelWidth = 0.04*Menu::getScaleFactor();
   for( int i=0; i<m_ItemsCount; i++ )
   {
      m_pMenuItems[i]->invalidate();
      m_pMenuItems[i]->getItemHeight(getSelectionWidth());
      m_pMenuItems[i]->getTitleWidth(getSelectionWidth());
      m_pMenuItems[i]->getValueWidth(getSelectionWidth());
      if ( (i % m_iColumnsCount) == 0 )
         m_pMenuItems[i]->Render(m_RenderXPos + m_sfMenuPaddingX, y, i == m_SelectedIndex, fFirstItemWidth);
      else
      {
         float dx = fFirstItemWidth + m_sfMenuPaddingX;
         if ( (i % m_iColumnsCount) == 2 )
            dx += fSecItemWidth + m_sfMenuPaddingX;
         m_pMenuItems[i]->RenderCondensed(m_RenderXPos + m_sfMenuPaddingX + dx, y, i == m_SelectedIndex, 0);
      }
      if ( (i % m_iColumnsCount) == m_iColumnsCount-1 )
      {
         y += m_pMenuItems[i]->getItemHeight(getUsableWidth());
         y += MENU_ITEM_SPACING * height_text;
      }
   }
   RenderEnd(yTop);
}


void MenuVehicleRCFailsafe::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicleRCFailsafe::onSelectItem()
{
   int subIndex = m_SelectedIndex % m_iColumnsCount;

   if ( subIndex == 0 )
   {
      Menu::onSelectItem();
      return;
   }

   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   int index = m_SelectedIndex/m_iColumnsCount;
   int fstype = m_ItemsChannels[index].pItemType->getSelectedIndex();
   int fsvalue = m_ItemsChannels[index].pItemValue->getCurrentValue();

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   params.rcChFlags[index] &= ~(0x07<<1);
   params.rcChFlags[index] |= ((fstype+1)<<1);
   params.rcChFailSafe[index] = fsvalue;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();  
}