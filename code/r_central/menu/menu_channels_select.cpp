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

#include "../../base/base.h"
#include "../../base/models.h"
#include "../../base/config.h"
#include "../../base/commands.h"
#include "../../common/string_utils.h"
#include "menu.h"
#include "menu_channels_select.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

#include "../shared_vars.h"
#include "../pairing.h"
#include "../ruby_central.h"

u32 s_uChannelsSelect433Band = 0;
u32 s_uChannelsSelect868Band = 0;
u32 s_uChannelsSelect23Band = 0;
u32 s_uChannelsSelect24Band = 0;
u32 s_uChannelsSelect25Band = 0;
u32 s_uChannelsSelect58Band = 0;

MenuChannelsSelect::MenuChannelsSelect(u32 uFrequencyBands)
:Menu(MENU_ID_CHANNELS_SELECT, "Select Frequencies", NULL)
{
   m_Width = 0.45;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.20;
   
   setColumnsCount(4);
   enableColumnSelection(false);
   
   addMenuItem(new MenuItem("Select All", ""));
   addMenuItem(new MenuItem("Deselect All", ""));

   m_uFrequencyBands = uFrequencyBands;
   char szBuff[64];

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
      {
         u32 freq = getChannels23()[i];
         strcpy(szBuff, str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect23Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
      {
         u32 freq = getChannels24()[i];
         strcpy(szBuff, str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect24Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
      {
         u32 freq = getChannels25()[i];
         strcpy(szBuff,str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect25Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
      {
         u32 freq = getChannels58()[i];
         strcpy(szBuff, str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect58Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   m_ExtraItemsHeight += 2.0 * m_pMenuItems[0]->getItemHeight(getUsableWidth());
}

MenuChannelsSelect::~MenuChannelsSelect()
{
}

void MenuChannelsSelect::valuesToUI()
{
   int itemIndex = 2;

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect23Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      } 

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect24Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      } 

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect25Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      } 

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect58Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      } 
}

void MenuChannelsSelect::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   y += RenderItem(0,y);
   y += RenderItem(1,y);

   float yStart = y;
   int quarter = (m_ItemsCount-2)/4;
   for( int i=0; i<quarter; i++ )
      y += RenderItem(i+2,y);

   y = yStart;
   for( int i=quarter; i<2*quarter; i++ )
      y += RenderItem(i+2, y, getUsableWidth() + m_sfMenuPaddingX);

   y = yStart;
   for( int i=2*quarter; i<3*quarter; i++ )
      y += RenderItem(i+2, y, 2*(getUsableWidth() + m_sfMenuPaddingX));

   y = yStart;
   for( int i=3*quarter; i<m_ItemsCount-2; i++ )
      y += RenderItem(i+2, y, 3*(getUsableWidth() + m_sfMenuPaddingX));

   RenderEnd(yTop);
}

int MenuChannelsSelect::onBack()
{
   int itemIndex = 2;

   s_uChannelsSelect23Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect23Band |= (((u32)0x01)<<i);
         itemIndex++;
      } 

   s_uChannelsSelect24Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect24Band |= (((u32)0x01)<<i);
         itemIndex++;
      } 

   s_uChannelsSelect25Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect25Band |= (((u32)0x01)<<i);
         itemIndex++;
      } 

   s_uChannelsSelect58Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect58Band |= (((u32)0x01)<<i);
         itemIndex++;
      } 
   
   menu_stack_pop(1);
   return 1;
}

void MenuChannelsSelect::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( 0 == m_SelectedIndex )
   {
      s_uChannelsSelect433Band = MAX_U32;
      s_uChannelsSelect868Band = MAX_U32;
      s_uChannelsSelect23Band = MAX_U32;
      s_uChannelsSelect24Band = MAX_U32;
      s_uChannelsSelect25Band = MAX_U32;
      s_uChannelsSelect58Band = MAX_U32;
      valuesToUI();
   }
   if ( 1 == m_SelectedIndex )
   {
      s_uChannelsSelect433Band = 0;
      s_uChannelsSelect868Band = 0;
      s_uChannelsSelect23Band = 0;
      s_uChannelsSelect24Band = 0;
      s_uChannelsSelect25Band = 0;
      s_uChannelsSelect58Band = 0;
      valuesToUI();
   }
}
