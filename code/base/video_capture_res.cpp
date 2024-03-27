#include "base.h"
#include "config.h"
#include "video_capture_res.h"

type_video_capture_resolution_info g_listCaptureResolutions[] = 
{
   {569,320, 90, "320p"},
   {560,420, 90, "SD 4:3"},
   {746,420, 90, "SD"},
   {980,420, 90, "SD 21:9"},
   {800,420, 90, "480 DSI"},
   {853,480, 90, "480p"},
   {1120,480, 90, "480p 21:9"},
   {1024,576, 90, "576p"},
   {1280,720, 90, "720p"},
   {1680,720, 40, "720p 21:9"},
   {1296,730, 40, "730p"},
   {1200,900, 40, "900p 4:3"},
   {1635,920, 40, "920p"},
   {1280,960, 40, "960p 4:3"},
   {1920,1080, 40, "1080p"}
};
int g_iListCaptureResolutionsCount = 0;


type_video_capture_resolution_info g_listCaptureResolutionsVeye[] = 
{
   {1280,720, 30, "720p"},
   {1920,1080, 30, "1080p"}
};
int g_iListCaptureResolutionsVeyeCount = 2;

type_video_capture_resolution_info g_listCaptureResolutionsOpenIPC[] = 
{
   {1280,720, 120, "720p"},
   {1920,1080, 120, "1080p"}
};
int g_iListCaptureResolutionsOpenIPCCount = 2;

int getOptionsVideoResolutionsCount()
{
   if ( 0 == g_iListCaptureResolutionsCount )
      g_iListCaptureResolutionsCount = sizeof(g_listCaptureResolutions)/sizeof(g_listCaptureResolutions[0]);
   return g_iListCaptureResolutionsCount;
}