#pragma once
#include "menu_objects.h"

#define MAX_SETTINGS_IMPORT_FILES 256

class MenuVehicleImport: public Menu
{
   public:
      MenuVehicleImport();
      virtual ~MenuVehicleImport();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();

      void buildSettingsFileList();
      int getSettingsFilesCount();

      void setCreateNew();

   protected:
      void addTopDescription();
      void addMessage(const char* szMessage);

      char* m_szTempFiles[MAX_SETTINGS_IMPORT_FILES];
      int m_TempFilesCount;
      bool m_bCreateNew;
};