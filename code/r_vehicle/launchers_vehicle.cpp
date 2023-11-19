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

#include "launchers_vehicle.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_i2c.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#include <math.h>

#include "shared_vars.h"
#include "timers.h"

bool s_bIsFirstCameraParamsUpdate = true;

static type_camera_parameters s_LastAppliedVeyeCameraParams;
static type_video_link_profile s_LastAppliedVeyeVideoParams;

static bool s_bAudioCaptureIsStarted = false;
static pthread_t s_pThreadAudioCapture;
static pthread_t s_pThreadBgAffinities;
static bool s_bThreadBgAffinitiesStarted = false;
static int s_iCPUCoresCount = -1;

void vehicle_launch_tx_telemetry(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching TX telemetry. Can't start TX telemetry.");
      return;
   }   
   hw_execute_bash_command("./ruby_tx_telemetry&", NULL);
}

void vehicle_stop_tx_telemetry()
{
   hw_stop_process("ruby_tx_telemetry");
}

void vehicle_launch_rx_rc(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching RX RC. Can't start RX RC.");
      return;
   }
   char szBuff[256];
   sprintf(szBuff, "ionice -c 1 -n %d nice -n %d ./ruby_rx_rc &", DEFAULT_IO_PRIORITY_RC, pModel->niceRC);
   hw_execute_bash_command(szBuff, NULL);
}

void vehicle_stop_rx_rc()
{
   hw_stop_process("ruby_rx_rc");
}

void vehicle_launch_rx_commands(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching RX commands. Can't start RX commands.");
      return;
   }
   hw_execute_bash_command("./ruby_rx_commands &", NULL);
}

void vehicle_stop_rx_commands()
{
   hw_stop_process("ruby_rx_commands");
}

void vehicle_launch_tx_router(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching TX video pipeline. Can't start TX video pipeline.");
      return;
   }

   hardware_sleep_ms(20);

   char szBuff[1024];
   if ( pModel->ioNiceRouter > 0 )
      sprintf(szBuff, "ionice -c 1 -n %d nice -n %d ./ruby_rt_vehicle &", pModel->ioNiceRouter, pModel->niceRouter );
   else
      sprintf(szBuff, "nice -n %d ./ruby_rt_vehicle &", pModel->niceRouter);

   hw_execute_bash_command(szBuff, NULL);
}

void vehicle_stop_tx_router()
{
   hw_stop_process("ruby_rt_vehicle");
}

bool vehicle_launch_video_capture(Model* pModel, shared_mem_video_link_overwrites* pVideoOverwrites)
{
   if ( NULL == pModel )
   {
      log_error_and_alarm("Tried to launch the video program without a model definition.");
      return false;
   }

   if ( s_bIsFirstCameraParamsUpdate )
   {
      memset((u8*)&s_LastAppliedVeyeCameraParams, 0, sizeof(type_camera_parameters));
      memset((u8*)&s_LastAppliedVeyeVideoParams, 0, sizeof(video_parameters_t));
   }

   if ( pModel->isActiveCameraVeye() )
   {
      char szComm[1024];
      char szOutput[1024];
 
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);

      sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("VEYE Camera Dev Id output: %s", szOutput);

      sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f hdver -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("VEYE Camera HW Ver output: %s", szOutput);

      vehicle_update_camera_params(pModel, pModel->iCurrentCamera);
      hardware_sleep_ms(200);
   }

   if ( pModel->isActiveCameraHDMI() )
      hardware_sleep_ms(200);

   log_line("Computing video capture parameters for active camera type: %d ...", pModel->getActiveCameraType());

   char szBuff[1024];
   char szCameraFlags[256];
   char szVideoFlags[256];
   char szPriority[64];
   szCameraFlags[0] = 0;
   szVideoFlags[0] = 0;
   szPriority[0] = 0;
   pModel->getCameraFlags(szCameraFlags);
   pModel->getVideoFlags(szVideoFlags, pModel->video_params.user_selected_video_link_profile, pVideoOverwrites);


   if ( pModel->ioNiceVideo > 0 )
      sprintf(szPriority, "ionice -c 1 -n %d nice -n %d", pModel->ioNiceVideo, pModel->niceVideo );
   else
      sprintf(szPriority, "nice -n %d", pModel->niceVideo );

   if ( pModel->isActiveCameraVeye() )
   {
      if ( pModel->camera_params[pModel->iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE307 )
      {
         if ( pModel->bDeveloperMode )
            sprintf(szBuff, "%s %s -dbg %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags, FIFO_RUBY_CAMERA1 );
         else
            sprintf(szBuff, "%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags, FIFO_RUBY_CAMERA1 );
      }
      else
      {
         if ( pModel->bDeveloperMode )
            sprintf(szBuff, "%s %s -dbg %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags, FIFO_RUBY_CAMERA1 );
         else
            sprintf(szBuff, "%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags, FIFO_RUBY_CAMERA1 );
      }
   }
   else
   {
      if ( pModel->bDeveloperMode )
         sprintf(szBuff, "%s ./%s -dbg %s %s -log -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, FIFO_RUBY_CAMERA1 );
      else
         sprintf(szBuff, "%s ./%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, FIFO_RUBY_CAMERA1 );
   }

   FILE* fd = fopen(FILE_TMP_CURRENT_VIDEO_PARAMS, "w");
   if ( NULL == fd )
      log_softerror_and_alarm("Failed to save current video config log to file: %s",FILE_TMP_CURRENT_VIDEO_PARAMS);
   else
   {
      fprintf(fd, "Video Flags: %s  # ", szVideoFlags);
      fprintf(fd, "Camera Profile: %s; Camera Flags: %s", model_getCameraProfileName(pModel->camera_params[pModel->iCurrentCamera].iCurrentProfile), szCameraFlags);
      fclose(fd);
   }

   //log_line("Executing video pipeline: [%s]", szBuff);
   bool bResult = hw_execute_bash_command(szBuff, NULL);

   if ( pModel->isActiveCameraVeye() )
   {
      if ( pModel->isActiveCameraVeye307() )
         hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE307, pModel->niceVideo, pModel->ioNiceVideo, 1 );
      else
         hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE, pModel->niceVideo, pModel->ioNiceVideo, 1 );
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, pModel->niceVideo, pModel->ioNiceVideo, 1 );
      g_TimeStartRaspiVid = g_TimeNow;
      g_bDidSentRaspividBitrateRefresh = false;
   }
   else
   {
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND, pModel->niceVideo, pModel->ioNiceVideo, 1 );
      g_TimeStartRaspiVid = g_TimeNow;
      g_bDidSentRaspividBitrateRefresh = false;
   }

   log_line("Completed launching video capture.");
   return bResult;
}

void vehicle_stop_video_capture(Model* pModel)
{
   g_SM_VideoLinkStats.overwrites.hasEverSwitchedToLQMode = 0;

   if ( pModel->isActiveCameraVeye() )
   {
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE);
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE307);
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME);
   }
   else
      hw_stop_process(VIDEO_RECORDER_COMMAND);

   g_TimeStartRaspiVid = 0;
   g_bDidSentRaspividBitrateRefresh = true;
   log_line("Stopped video capture process.");
}


void vehicle_update_camera_params(Model* pModel, int iCameraIndex)
{
   if ( ! pModel->isActiveCameraVeye() )
      return;

   char szComm[1024];
   char szCameraFlags[512];
   szCameraFlags[0] = 0;
   pModel->getCameraFlags(szCameraFlags);
   if ( iCameraIndex < 0 || iCameraIndex >= pModel->iCameraCount )
      iCameraIndex = 0;

   int iProfile = pModel->camera_params[iCameraIndex].iCurrentProfile;
   bool bApplyAll = false;
   if ( s_LastAppliedVeyeVideoParams.width == 0 )
      bApplyAll = true;

   if ( s_bIsFirstCameraParamsUpdate )
   {
      memset((u8*)&s_LastAppliedVeyeCameraParams, 0, sizeof(type_camera_parameters));
      memset((u8*)&s_LastAppliedVeyeVideoParams, 0, sizeof(video_parameters_t));
      s_bIsFirstCameraParamsUpdate = false;
      bApplyAll = true;
   }

   if ( pModel->isActiveCameraVeye307() )
   {
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      
      if ( s_LastAppliedVeyeVideoParams.width != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width ||
           s_LastAppliedVeyeVideoParams.height != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height ||
           s_LastAppliedVeyeVideoParams.fps != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f  videofmt -p1 %d -p2 %d -p3 %d -b %d; cd $current_dir",
          VEYE_COMMANDS_FOLDER307, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x2 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f aetarget -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, (int)(2.5f*(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness)), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, pModel->camera_params[iCameraIndex].profiles[iProfile].contrast, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f satu -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || (fabs(s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].awbGainR - pModel->camera_params[iCameraIndex].profiles[iProfile].awbGainR) > 0.1) )
      {
         int hue = pModel->camera_params[iCameraIndex].profiles[iProfile].awbGainR;
         if ( (hue > 1) && (hue < 100) )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f hue -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, hue, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].shutterspeed != pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
            hw_execute_bash_command(szComm, NULL);

            char szShutter[24];
            sprintf(szShutter, "%d", (int)(1000000l/(long)pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed));
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f metime -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 3 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   else // IMX 327 camera
   {
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      if ( bApplyAll )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f  videofmt -p1 NTSC -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFF -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFE -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f brightness -p1 0x%02X -b %d 2>&1; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness), nBus);
         char szOutput[1024];
         szOutput[0] = 0;
         hw_execute_bash_command_raw(szComm, szOutput);
         log_line("output: [%s]", szOutput);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(2.5*pModel->camera_params[iCameraIndex].profiles[iProfile].contrast), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f saturation -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].wdr != pModel->camera_params[iCameraIndex].profiles[iProfile].wdr )
      {
         int wdrmode = (int) pModel->camera_params[iCameraIndex].profiles[iProfile].wdr;
         if ( wdrmode < 0 || wdrmode > 3 )
            wdrmode = 0;
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wdrmode -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, wdrmode, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].shutterspeed != pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 0x40 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
         {
         char szShutter[24];
         strcpy(szShutter, "0x40");
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 50 )
            strcpy(szShutter, "0x41"); // 1/30
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 100 )
            strcpy(szShutter, "0x42"); // 1/60
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 200 )
            strcpy(szShutter, "0x43"); // 1/120
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 400 )
            strcpy(szShutter, "0x44"); // 1/240
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 700 )
            strcpy(szShutter, "0x45"); // 1/480
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 1600 )
            strcpy(szShutter, "0x46"); // 1/1000
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 3500 )
            strcpy(szShutter, "0x47"); // 1/2000
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 7000 )
            strcpy(szShutter, "0x48"); // 1/5000
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 20000 )
            strcpy(szShutter, "0x49"); // 1/10000
         else
            strcpy(szShutter, "0x4A"); // 1/50000

         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x1B -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x18 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].sharpness != pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness >= 100 )
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness <= 110 )
         {
            if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness == 100 )
               sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x0 -p2 0x0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
            else
               sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x1 -p2 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness - 100, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].drc != pModel->camera_params[iCameraIndex].profiles[iProfile].drc )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].drc<16 )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f agc -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].drc, nBus );
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x03 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x00 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   memcpy((u8*)&s_LastAppliedVeyeCameraParams, (u8*)&(pModel->camera_params[iCameraIndex]), sizeof(type_camera_parameters));
   memcpy((u8*)&s_LastAppliedVeyeVideoParams, (u8*)&(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile]), sizeof(type_video_link_profile));
}


static void * _thread_audio_capture(void *argument)
{
   s_bAudioCaptureIsStarted = true;

   Model* pModel = (Model*) argument;
   if ( NULL == pModel )
   {
      s_bAudioCaptureIsStarted = false;
      return NULL;
   }

   char szCommFlag[256];
   char szCommCapture[256];
   char szRate[32];
   char szPriority[64];
   int iIntervalSec = 5;

   sprintf(szCommCapture, "amixer -c 1 sset Mic %d%%", pModel->audio_params.volume);
   hw_execute_bash_command(szCommCapture, NULL);

   strcpy(szRate, "8000");
   if ( 0 == pModel->audio_params.quality )
      strcpy(szRate, "14000");
   if ( 1 == pModel->audio_params.quality )
      strcpy(szRate, "24000");
   if ( 2 == pModel->audio_params.quality )
      strcpy(szRate, "32000");
   if ( 3 == pModel->audio_params.quality )
      strcpy(szRate, "44100");

   strcpy(szRate, "44100");

   szPriority[0] = 0;
   if ( pModel->ioNiceVideo > 0 )
      sprintf(szPriority, "ionice -c 1 -n %d nice -n %d", pModel->ioNiceVideo, pModel->niceVideo );
   else
      sprintf(szPriority, "nice -n %d", pModel->niceVideo );

   sprintf(szCommCapture, "%s arecord --device=hw:1,0 --file-type wav --format S16_LE --rate %s -c 1 -d %d -q >> %s",
      szPriority, szRate, iIntervalSec, FIFO_RUBY_AUDIO1);

   sprintf(szCommFlag, "echo '0123456789' > %s", FIFO_RUBY_AUDIO1);

   hw_stop_process("arecord");

   while ( s_bAudioCaptureIsStarted && ( ! g_bQuit) )
   {
      if ( g_bReinitializeRadioInProgress )
      {
         hardware_sleep_ms(50);
         continue;         
      }

      u32 uTimeCheck = get_current_timestamp_ms();

      hw_execute_bash_command(szCommCapture, NULL);
      
      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow < uTimeCheck + (u32)iIntervalSec * 500 )
      {
         log_softerror_and_alarm("[AudioCaptureThread] Audio capture segment finished too soon (took %u ms, expected %d ms)",
             uTimeNow - uTimeCheck, iIntervalSec*1000);
         hardware_sleep_ms(iIntervalSec*500);
      }

      hw_execute_bash_command(szCommFlag, NULL);
   }
   s_bAudioCaptureIsStarted = false;
   return NULL;
}

void vehicle_launch_audio_capture(Model* pModel)
{
   if ( s_bAudioCaptureIsStarted )
      return;

   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) || (! pModel->audio_params.enabled) )
      return;

   if ( 0 != pthread_create(&s_pThreadAudioCapture, NULL, &_thread_audio_capture, g_pCurrentModel) )
   {
      log_softerror_and_alarm("Failed to create thread for audio capture.");
      s_bAudioCaptureIsStarted = false;
      return;
   }
   s_bAudioCaptureIsStarted = true;
   log_line("Created thread for audio capture.");
}

void vehicle_stop_audio_capture(Model* pModel)
{
   if ( ! s_bAudioCaptureIsStarted )
      return;
   s_bAudioCaptureIsStarted = false;

   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) )
      return;

   //hw_execute_bash_command("kill -9 $(pidof arecord) 2>/dev/null", NULL);
   hw_stop_process("arecord");
}

static void * _thread_adjust_affinities_vehicle(void *argument)
{
   s_bThreadBgAffinitiesStarted = true;
   log_line("Started background thread to adjust processes affinities...");
   
   if ( s_iCPUCoresCount < 1 )
   {
      s_iCPUCoresCount = 1;
      char szOutput[128];
      hw_execute_bash_command_raw("nproc --all", szOutput);
      if ( 1 != sscanf(szOutput, "%d", &s_iCPUCoresCount) )
      {
         log_softerror_and_alarm("Failed to get CPU cores count. No affinity adjustments for processes to be done.");
         s_bThreadBgAffinitiesStarted = false;
         s_iCPUCoresCount = 1;
         return NULL;    
      }
   }

   if ( s_iCPUCoresCount < 2 || s_iCPUCoresCount > 32 )
   {
      log_line("Single core CPU (%d), no affinity adjustments for processes to be done.", s_iCPUCoresCount);
      s_bThreadBgAffinitiesStarted = false;
      return NULL;
   }

   log_line("%d CPU cores, doing affinity adjustments for processes...", s_iCPUCoresCount);
   if ( s_iCPUCoresCount > 2 )
   {
      hw_set_proc_affinity("ruby_rt_vehicle", 1,1);
      hw_set_proc_affinity("ruby_tx_telemetry", 2,2);
      hw_set_proc_affinity("ruby_rx_rc", 2,2);
      hw_set_proc_affinity("ruby_capture_raspi", 3, s_iCPUCoresCount);
   }
   else
   {
      hw_set_proc_affinity("ruby_rt_vehicle", 1,1);
      hw_set_proc_affinity("ruby_capture_raspi", 2, s_iCPUCoresCount);
   }
   log_line("Background thread to adjust processes affinities completed.");
   s_bThreadBgAffinitiesStarted = false;
   return NULL;
}

void vehicle_check_update_processes_affinities(bool bUseThread)
{
   log_line("Adjust processes affinities. Use thread: %s", (bUseThread?"Yes":"No"));

   if ( ! bUseThread )
   {
      _thread_adjust_affinities_vehicle(NULL);
      log_line("Adjusted processes affinities");
      return;
   }
   
   if ( s_bThreadBgAffinitiesStarted )
   {
      log_line("A background thread to adjust processes affinities is already running. Do nothing.");
      return;
   }
   s_bThreadBgAffinitiesStarted = true;
   if ( 0 != pthread_create(&s_pThreadBgAffinities, NULL, &_thread_adjust_affinities_vehicle, NULL) )
   {
      log_error_and_alarm("Failed to create thread for adjusting processes affinities.");
      s_bThreadBgAffinitiesStarted = false;
      return;
   }

   log_line("Created thread for adjusting processes affinities");
}
