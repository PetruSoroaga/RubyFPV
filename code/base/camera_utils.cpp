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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"


int camera_get_active_camera_h264_slices(Model* pModel)
{
   if ( NULL == pModel )
      return 1;

   if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width > 1280 )
      return 1;
   if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height > 720 )
      return 1;

   return pModel->video_params.iH264Slices;
}