#pragma once

typedef struct
{
   int iWidth;
   int iHeight;
   int iMaxFPS;
   char szName[24];
} type_video_capture_resolution_info;


type_video_capture_resolution_info* getOptionsVideoResolutions(int iCameraType);
int getOptionsVideoResolutionsCount(int iCameraType);
int getOptionsVideoResolutionMaxFPS(int iCameraType, int w, int h);
char* getOptionVideoResolutionName(int w, int h);
