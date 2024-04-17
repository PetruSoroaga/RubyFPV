#pragma once

typedef struct
{
   int iWidth;
   int iHeight;
   int iMaxFPS;
   char szName[24];
} type_video_capture_resolution_info;


extern type_video_capture_resolution_info g_listCaptureResolutions[];
extern int g_iListCaptureResolutionsCount;

extern type_video_capture_resolution_info g_listCaptureResolutionsVeye[];
extern int g_iListCaptureResolutionsVeyeCount;

extern type_video_capture_resolution_info g_listCaptureResolutionsVeye307[];
extern int g_iListCaptureResolutionsVeyeCount307;

extern type_video_capture_resolution_info g_listCaptureResolutionsOpenIPC[];
extern int g_iListCaptureResolutionsOpenIPCCount;

int getOptionsVideoResolutionsCount();
