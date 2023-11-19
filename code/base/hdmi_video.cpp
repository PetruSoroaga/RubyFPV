#include "base.h"
#include "config.h"
#include "hdmi_video.h"

int g_iOptionsVideoIndex720p = 8;
int g_iOptionsVideoIndex1080p = 14;
int g_iOptionsVideoWidth[] =  { 569,      560,    746,     980,       800,       853,     1120,     1024,   1280,     1680,      1296,    1200,     1635,     1280,      1920 }; //, 2560 };
int g_iOptionsVideoHeight[] = { 320,      420,    420,     420,       480,       480,      480,      576,    720,      720,       730,     900,      920,      960,      1080 }; //, 1440 };
int g_iOptionsVideoMaxFPS[] = { 90,        90,     90,      90,        90,        90,       90,       90,     90,       40,        40,      40,       40,       40,        40 };
const char* g_szOptionsVideoRes[] = 
                              { "320p", "SD 4:3", "SD", "SD 21:9", "480 DSI",  "480p", "480p 21:9", "576p", "720p", "720p 21:9", "730p", "900p 4:3", "920p", "960p 4:3", "1080p"};


int getOptionsVideoResolutionsCount()
{
   return sizeof(g_iOptionsVideoWidth)/sizeof(g_iOptionsVideoWidth[0]);
}