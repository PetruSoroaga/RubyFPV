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

#include "../../public/settings_info.h"
#include "menu.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../osd/osd_common.h"
#include "../osd/osd_plugins.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define PLUGIN_MOVE_MARGIN 0.01

extern bool s_bMenuOSDPluginsPluginDeleted;

MenuVehicleOSDPlugin::MenuVehicleOSDPlugin(int pluginIndex)
:Menu(MENU_ID_VEHICLE_OSD_PLUGIN, "Plugin Settings", NULL)
{
   m_Width = 0.22;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.22;
   m_nPluginIndex = pluginIndex;
   m_nPluginSettingsCount = 0;
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;

   m_fMenuOrgAlpha = menu_getGlobalAlpha();
   readPlugin();
}

MenuVehicleOSDPlugin::~MenuVehicleOSDPlugin()
{
}

void MenuVehicleOSDPlugin::readPlugin()
{
   removeAllItems();

   char* szName = osd_plugins_get_name(m_nPluginIndex);
   char szBuff[256];
   sprintf(szBuff, "Configure %s", szName);
   setTitle(szBuff);

   int iMaxStringLenght = 0;
   int iMaxStringLenght2 = 0;

   for( int i=0; i<MAX_PLUGIN_SETTINGS; i++ )
      m_IndexSettings[i] = -1;

   m_nPluginSettingsCount = osd_plugin_get_settings_count(m_nPluginIndex);
   if ( m_nPluginSettingsCount > MAX_PLUGIN_SETTINGS )
      m_nPluginSettingsCount = MAX_PLUGIN_SETTINGS;

   for( int i=0; i<m_nPluginSettingsCount; i++ )
   {
      int type = osd_plugin_get_setting_type(m_nPluginIndex, i);
      char* szName = osd_plugin_get_setting_name(m_nPluginIndex,i);
      if ( strlen(szName) > iMaxStringLenght )
         iMaxStringLenght = strlen(szName);
      if ( type == PLUGIN_SETTING_TYPE_BOOL )
      {
         m_pItemsSelect[i+1] = new MenuItemSelect(szName);
         m_pItemsSelect[i+1]->addSelection("No");
         m_pItemsSelect[i+1]->addSelection("Yes");
         //m_pItemsSelect[i+1]->setIsEditable();
         //m_pItemsSelect[i+1]->setUseMultiViewLayout();
         m_IndexSettings[i] = addMenuItem(m_pItemsSelect[i+1]);
      }
      if ( type == PLUGIN_SETTING_TYPE_ENUM )
      {
         m_pItemsSelect[i+1] = new MenuItemSelect(szName);
         int c = osd_plugin_get_setting_options_count(m_nPluginIndex, i);
         for( int k=0; k<c; k++ )
         {
            char* szOptName = osd_plugin_get_setting_option_name(m_nPluginIndex, i, k);
            if ( NULL != szOptName )
            {
               if ( strlen(szOptName) > iMaxStringLenght2 )
                  iMaxStringLenght2 = strlen(szOptName);
               m_pItemsSelect[i+1]->addSelection(szOptName);
            }
         }
         m_pItemsSelect[i+1]->setIsEditable();
         m_IndexSettings[i] = addMenuItem(m_pItemsSelect[i+1]);
      }
      if ( type == PLUGIN_SETTING_TYPE_INT )
      {
         int min = osd_plugin_get_setting_minvalue(m_nPluginIndex,i);
         int max = osd_plugin_get_setting_maxvalue(m_nPluginIndex,i);
         int def = osd_plugin_get_setting_defaultvalue(m_nPluginIndex,i);
         m_pItemsRange[i+1] = new MenuItemRange(szName, min, max, def, 1 );  
         m_pItemsRange[i+1]->setSufix("");
         m_IndexSettings[i] = addMenuItem(m_pItemsRange[i+1]);
      }
   }

   addMenuItem(new MenuItemSection("Layout"));

   m_IndexMoveX = addMenuItem(new MenuItem("Move Horizontally", "Sets the horizontal position of the plugin on the screen."));
   m_IndexMoveY = addMenuItem(new MenuItem("Move Vertically", "Sets the horizontal position of the plugin on the screen."));
   m_IndexResize = addMenuItem(new MenuItem("Resize", "Sets the size of the plugin on the screen."));


   iMaxStringLenght += iMaxStringLenght2;
   for( int i=0; i<iMaxStringLenght; i++ )
      szBuff[i] = 'A';
   szBuff[iMaxStringLenght] = 0;

   float fMaxWidth = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);

   fMaxWidth -= m_sfMenuPaddingX;
   if ( fMaxWidth > 0.6 )
      fMaxWidth = 0.6;
   if ( fMaxWidth > m_Width )
   {
      m_Width = fMaxWidth;
      invalidate();
   }
}

void MenuVehicleOSDPlugin::valuesToUI()
{
   PluginsSettings* pPS = get_PluginsSettings();

   char* szUID = osd_plugins_get_uid(m_nPluginIndex);
   SinglePluginSettings* pPlugin = pluginsGetByUID(szUID);
   if ( NULL == pPlugin )
      return;

   int iModelIndex = getPluginModelSettingsIndex(pPlugin, g_pCurrentModel);
   int osdLayoutIndex = g_pCurrentModel->osd_params.layout;

   m_nPluginSettingsCount = osd_plugin_get_settings_count(m_nPluginIndex);
   if ( m_nPluginSettingsCount > MAX_PLUGIN_SETTINGS )
      m_nPluginSettingsCount = MAX_PLUGIN_SETTINGS;
      
   for( int i=0; i<m_nPluginSettingsCount; i++ )
   {
      int type = osd_plugin_get_setting_type(m_nPluginIndex, i);
      if ( type == PLUGIN_SETTING_TYPE_BOOL )
      {
         m_pItemsSelect[i+1]->setSelectedIndex( pPlugin->nSettings[iModelIndex][osdLayoutIndex][i] );
      }
      if ( type == PLUGIN_SETTING_TYPE_ENUM )
      {
         m_pItemsSelect[i+1]->setSelectedIndex( pPlugin->nSettings[iModelIndex][osdLayoutIndex][i] );
      }
      if ( type == PLUGIN_SETTING_TYPE_INT )
      {
         m_pItemsRange[i+1]->setCurrentValue( pPlugin->nSettings[iModelIndex][osdLayoutIndex][i] );
      }
   }
}

void MenuVehicleOSDPlugin::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleOSDPlugin::stopAction()
{
   m_bIsMovingH = false;
   m_bIsMovingV = false;
   m_bIsResizing = false;
   menu_setGlobalAlpha(m_fMenuOrgAlpha);

   plugin_osd_t* pPlugin = osd_plugins_get(m_nPluginIndex);
   pPlugin->bBoundingBox = false;
   pPlugin->bHighlight = false;

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      plugin_osd_t* pPlugin = osd_plugins_get(i);
      if ( NULL != pPlugin )
         pPlugin->bBoundingBox = false;
   }
   save_PluginsSettings();
}

bool MenuVehicleOSDPlugin::periodicLoop()
{
   return false;
}


void MenuVehicleOSDPlugin::onMoveUp(bool bIgnoreReversion)
{

   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      if ( m_bIsMovingH )
      if ( osd_getMarginX() + pPluginSettings->fXPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginX()) + pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] <= 1.0-osd_getMarginX()-PLUGIN_MOVE_MARGIN )
         pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN;

      if ( m_bIsMovingV )
      if ( pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] >= PLUGIN_MOVE_MARGIN )
         pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN;

      float fAspect = 1.0;
      if ( 0 < pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] )
         fAspect = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] / pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];

      if ( m_bIsResizing )
      if ( osd_getMarginX() + pPluginSettings->fXPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginX()) + pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] < (1.0-PLUGIN_MOVE_MARGIN) )
      if ( osd_getMarginY() + pPluginSettings->fYPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginY()) + pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] < 1.0-PLUGIN_MOVE_MARGIN/fAspect )
      {
         pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN;
         pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN/fAspect;

         pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN*0.5;
         if ( pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] < PLUGIN_MOVE_MARGIN )
            pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] = PLUGIN_MOVE_MARGIN;

         pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN*0.5/fAspect;
         if ( pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] < PLUGIN_MOVE_MARGIN )
            pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] = PLUGIN_MOVE_MARGIN;
      }
      return;
   }
   Menu::onMoveUp(bIgnoreReversion);
}

void MenuVehicleOSDPlugin::onMoveDown(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      if ( m_bIsMovingH )
      if ( pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] >= PLUGIN_MOVE_MARGIN )
         pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN;

      if ( m_bIsMovingV )
      if ( osd_getMarginY() + pPluginSettings->fYPos[iModelIndex][osdLayoutIndex]*(1.0-2.0*osd_getMarginY()) + pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] <= 1.0-osd_getMarginY()-PLUGIN_MOVE_MARGIN )
         pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN;

      float fAspect = 1.0;
      if ( 0 < pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] )
         fAspect = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] / pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];

      if ( m_bIsResizing )
      if ( pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] > PLUGIN_MOVE_MARGIN )
      if ( pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] > PLUGIN_MOVE_MARGIN/fAspect )
      {
         pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN;
         pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] -= PLUGIN_MOVE_MARGIN/fAspect;

         pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN*0.5;
         pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] += PLUGIN_MOVE_MARGIN*0.5/fAspect;
      }
      return;
   }
   Menu::onMoveDown(bIgnoreReversion);
}

void MenuVehicleOSDPlugin::onMoveLeft(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      onMoveUp(bIgnoreReversion);
   }
   Menu::onMoveLeft(bIgnoreReversion);
}

void MenuVehicleOSDPlugin::onMoveRight(bool bIgnoreReversion)
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      onMoveDown(bIgnoreReversion);
   }
   Menu::onMoveRight(bIgnoreReversion);
}



int MenuVehicleOSDPlugin::onBack()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return 0;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return 0;

      int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      pPluginSettings->fXPos[iModelIndex][osdLayoutIndex] = m_fXOrg;
      pPluginSettings->fYPos[iModelIndex][osdLayoutIndex] = m_fYOrg;
      pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] = m_fWOrg;
      pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] = m_fHOrg;

      if ( pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] < 0.02 )
         pPluginSettings->fWidth[iModelIndex][osdLayoutIndex] = 0.02;
      if ( pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] < 0.02 )
         pPluginSettings->fHeight[iModelIndex][osdLayoutIndex] = 0.02;
      stopAction();
      return 1;
   }
   return 0;
}


void MenuVehicleOSDPlugin::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   m_iConfirmationId = 0;
}


void MenuVehicleOSDPlugin::onSelectItem()
{
   if ( m_bIsMovingH || m_bIsMovingV || m_bIsResizing )
   {
      stopAction();
      save_PluginsSettings();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexMoveX == m_SelectedIndex )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      m_fXOrg = pPluginSettings->fXPos[iModelIndex][osdLayoutIndex];
      m_fYOrg = pPluginSettings->fYPos[iModelIndex][osdLayoutIndex];
      m_fWOrg = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex];
      m_fHOrg = pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];
      pPluginOSD->bBoundingBox = true;
      pPluginOSD->bHighlight = true;

      for( int i=0; i<osd_plugins_get_count(); i++ )
      {
         plugin_osd_t* pPlugin = osd_plugins_get(i);
         if ( NULL != pPlugin )
            pPlugin->bBoundingBox = true;
      }

      m_bIsMovingH = true;
      menu_setGlobalAlpha(0.2);
      return;
   }
   if ( m_IndexMoveY == m_SelectedIndex )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      m_fXOrg = pPluginSettings->fXPos[iModelIndex][osdLayoutIndex];
      m_fYOrg = pPluginSettings->fYPos[iModelIndex][osdLayoutIndex];
      m_fWOrg = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex];
      m_fHOrg = pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];
      pPluginOSD->bBoundingBox = true;
      pPluginOSD->bHighlight = true;

      for( int i=0; i<osd_plugins_get_count(); i++ )
      {
         plugin_osd_t* pPlugin = osd_plugins_get(i);
         if ( NULL != pPlugin )
            pPlugin->bBoundingBox = true;
      }

      m_bIsMovingV = true;
      menu_setGlobalAlpha(0.2);
      return;
   }

   if ( m_IndexResize == m_SelectedIndex )
   {
      plugin_osd_t* pPluginOSD = osd_plugins_get(m_nPluginIndex);
      if ( NULL == pPluginOSD )
         return;

      SinglePluginSettings* pPluginSettings = osd_get_settings_for_plugin_for_model(pPluginOSD->szUID, g_pCurrentModel);
      if ( NULL == pPluginSettings )
         return;

      int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
      int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

      m_fXOrg = pPluginSettings->fXPos[iModelIndex][osdLayoutIndex];
      m_fYOrg = pPluginSettings->fYPos[iModelIndex][osdLayoutIndex];
      m_fWOrg = pPluginSettings->fWidth[iModelIndex][osdLayoutIndex];
      m_fHOrg = pPluginSettings->fHeight[iModelIndex][osdLayoutIndex];
      pPluginOSD->bBoundingBox = true;
      pPluginOSD->bHighlight = true;

      for( int i=0; i<osd_plugins_get_count(); i++ )
      {
         plugin_osd_t* pPlugin = osd_plugins_get(i);
         if ( NULL != pPlugin )
            pPlugin->bBoundingBox = true;
      }
      m_bIsResizing = true;
      menu_setGlobalAlpha(0.2);
      return;
   }

   m_nPluginSettingsCount = osd_plugin_get_settings_count(m_nPluginIndex);
   if ( m_nPluginSettingsCount > MAX_PLUGIN_SETTINGS )
      m_nPluginSettingsCount = MAX_PLUGIN_SETTINGS;

   for( int i=0; i<m_nPluginSettingsCount; i++ )
   {
      if ( m_SelectedIndex == m_IndexSettings[i] )
      {
         char* szUID = osd_plugins_get_uid(m_nPluginIndex);
         SinglePluginSettings* pPluginSettings = pluginsGetByUID(szUID);
         if ( NULL == pPluginSettings )
            return;

         int osdLayoutIndex = g_pCurrentModel->osd_params.layout;
         int iModelIndex = getPluginModelSettingsIndex(pPluginSettings, g_pCurrentModel);

         int type = osd_plugin_get_setting_type(m_nPluginIndex, i);

         if ( type == PLUGIN_SETTING_TYPE_BOOL )
         {
            pPluginSettings->nSettings[iModelIndex][osdLayoutIndex][i] = m_pItemsSelect[i+1]->getSelectedIndex();
         }
         if ( type == PLUGIN_SETTING_TYPE_ENUM )
         {
            pPluginSettings->nSettings[iModelIndex][osdLayoutIndex][i] = m_pItemsSelect[i+1]->getSelectedIndex();
         }
         if ( type == PLUGIN_SETTING_TYPE_INT )
         {
            pPluginSettings->nSettings[iModelIndex][osdLayoutIndex][i] = m_pItemsRange[i+1]->getCurrentValue();
         }
         save_PluginsSettings();
         valuesToUI();
         return;
      }
   }
}
