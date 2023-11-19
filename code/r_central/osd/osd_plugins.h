#pragma once
#include "../shared_vars.h"

// The info in OSD plugins structure is not persistent, is created only at runtime.
// The persistent info about a plugin is stored in SinglePluginSettings, in common plugin_settings file

typedef struct
{
   char szUID[MAX_PLUGIN_NAME_LENGTH];
   char szPluginFile[256];

   void* pLibrary;
   void (*pFunctionInit)(void*);
   char* (*pFunctionGetName)(void);
   int (*pFunctionGetVersion)(void);
   char* (*pFunctionGetUID)(void);
   float (*pFunctionGetDefaultWidth)(void);
   float (*pFunctionGetDefaultHeight)(void);
   void (*pFunctionRender)(vehicle_and_telemetry_info_t*, plugin_settings_info_t2*, float, float, float, float);

   // Optional functions:

   int (*pFunctionGetPluginSettingsCount)(void);
   char* (*pFunctionGetPluginSettingName)(int);
   int (*pFunctionGetPluginSettingType)(int);
   int (*pFunctionGetPluginSettingMinValue)(int);
   int (*pFunctionGetPluginSettingMaxValue)(int);
   int (*pFunctionGetPluginSettingDefaultValue)(int);
   int (*pFunctionGetPluginSettingOptionsCount)(int);
   char* (*pFunctionGetPluginSettingOptionName)(int, int);
   void (*pFunctionOnNewVehicle)(u32);
   int  (*pFunctionRequestTelemetryStreams)(void);
   void (*pFunctionOnTelemetryStreamData)(u8*, int, int);

   bool bBoundingBox;
   bool bHighlight;
} __attribute__((packed)) plugin_osd_t;

extern plugin_osd_t* g_pPluginsOSD[MAX_OSD_PLUGINS];
extern int g_iPluginsOSDCount;
extern bool g_bOSDPluginsNeedTelemetryStreams;

void osd_plugins_load();
void osd_plugins_render();

int osd_plugins_get_count();
plugin_osd_t* osd_plugins_get(int index);

char* osd_plugins_get_name(int index);
char* osd_plugins_get_short_name(int index);
char* osd_plugins_get_uid(int index);
void osd_plugins_delete(int index);

SinglePluginSettings* osd_get_settings_for_plugin_for_model(const char* szPluginUID, Model* pModel);

int osd_plugin_get_settings_count(int index );
char* osd_plugin_get_setting_name(int index, int sub_index);
int osd_plugin_get_setting_type(int index, int sub_index);
int osd_plugin_get_setting_minvalue(int index, int sub_index);
int osd_plugin_get_setting_maxvalue(int index, int sub_index);
int osd_plugin_get_setting_defaultvalue(int index, int sub_index);
int osd_plugin_get_setting_options_count(int index, int sub_index);
char* osd_plugin_get_setting_option_name(int index, int sub_index, int option_index);
