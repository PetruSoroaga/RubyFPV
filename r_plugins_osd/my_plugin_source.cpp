#include <stdio.h>
#include <math.h>
#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"

PLUGIN_VAR RenderEngineUI* g_pEngine = NULL;
PLUGIN_VAR const char* g_szPluginName = "My Example Plugin";
PLUGIN_VAR const char* g_szUID = "A342AST-A23432Q-WE19J-0024";

#ifdef __cplusplus
extern "C" {
#endif

void init(void* pEngine)
{
  g_pEngine = (RenderEngineUI*)pEngine;
}

char* getName()
{
  return (char*)g_szPluginName;
}

char* getUID()
{
  return (char*)g_szUID;
}

void render(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t2* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight)
{
  if ( NULL == g_pEngine || NULL == pTelemetryInfo )
  return;

  float xCenter = xPos + 0.5*fWidth;
  float yCenter = yPos + 0.5*fHeight;

  g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
  g_pEngine->drawCircle(xCenter, yCenter, fWidth * 0.5);

  char szBuff[64];
  sprintf(szBuff, "%d", pTelemetryInfo->heading);

  u32 fontId = g_pEngine->getFontIdRegular();
  float height_text = g_pEngine->textHeight(fontId);
  float width_text = g_pEngine->textWidth(fontId, szBuff);

  g_pEngine->drawText(xCenter - 0.5 * width_text, yCenter - 0.5 * height_text, fontId, szBuff);
}

#ifdef __cplusplus
}
#endif