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
#include "../base/launchers.h"

#include "shared_vars.h"
#include "timers.h"

bool s_bIsFirstCameraParamsUpdate = true;

static type_camera_parameters s_LastAppliedVeyeCameraParams;
static type_video_link_profile s_LastAppliedVeyeVideoParams;


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
   snprintf(szBuff, sizeof(szBuff), "ionice -c 1 -n %d nice -n %d ./ruby_rx_rc &", DEFAULT_IO_PRIORITY_RC, pModel->niceRC);
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
      snprintf(szBuff, sizeof(szBuff), "ionice -c 1 -n %d nice -n %d ./ruby_rt_vehicle &", pModel->ioNiceRouter, pModel->niceRouter );
   else
      snprintf(szBuff, sizeof(szBuff), "nice -n %d ./ruby_rt_vehicle &", pModel->niceRouter);

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

   if ( pModel->isCameraVeye() )
   {
      char szComm[1024];
      char szOutput[1024];
 
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);

      snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("VEYE Camera Dev Id output: %s", szOutput);

      snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f hdver -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("VEYE Camera HW Ver output: %s", szOutput);

      vehicle_update_camera_params(pModel, pModel->iCurrentCamera);
      hardware_sleep_ms(200);
   }

   if ( pModel->isCameraHDMI() )
      hardware_sleep_ms(200);

   log_line("Computing video capture parameters for camera type: %d ...", pModel->getCameraType());

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
      snprintf(szPriority, sizeof(szPriority), "ionice -c 1 -n %d nice -n %d", pModel->ioNiceVideo, pModel->niceVideo );
   else
      snprintf(szPriority, sizeof(szPriority), "nice -n %d", pModel->niceVideo );

   if ( pModel->isCameraVeye() )
   {
      if ( pModel->camera_params[pModel->iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE307 )
      {
         if ( pModel->bDeveloperMode )
            snprintf(szBuff, sizeof(szBuff), "%s %s -dbg %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags, FIFO_RUBY_CAMERA1 );
         else
            snprintf(szBuff, sizeof(szBuff), "%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags, FIFO_RUBY_CAMERA1 );
      }
      else
      {
         if ( pModel->bDeveloperMode )
            snprintf(szBuff, sizeof(szBuff), "%s %s -dbg %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags, FIFO_RUBY_CAMERA1 );
         else
            snprintf(szBuff, sizeof(szBuff), "%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags, FIFO_RUBY_CAMERA1 );
      }
   }
   else
   {
      if ( pModel->bDeveloperMode )
         snprintf(szBuff, sizeof(szBuff), "%s ./%s -dbg %s %s -log -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, FIFO_RUBY_CAMERA1 );
      else
         snprintf(szBuff, sizeof(szBuff), "%s ./%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, FIFO_RUBY_CAMERA1 );
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

   log_line("Executing video pipeline: [%s]", szBuff);
   bool bResult = hw_execute_bash_command(szBuff, NULL);

   if ( pModel->isCameraVeye() )
   {
      if ( pModel->isCameraVeye307() )
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

   if ( pModel->isCameraVeye() )
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
   hardware_sleep_ms(20);
}


void vehicle_update_camera_params(Model* pModel, int iCameraIndex)
{
   if ( ! pModel->isCameraVeye() )
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

   if ( pModel->isCameraVeye307() )
   {
      if ( s_LastAppliedVeyeVideoParams.width != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width ||
           s_LastAppliedVeyeVideoParams.height != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height ||
           s_LastAppliedVeyeVideoParams.fps != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f  videofmt -p1 %d -p2 %d -p3 %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x1; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x2; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f aetarget -p1 0x%02X; cd $current_dir", VEYE_COMMANDS_FOLDER307, (int)(2.5f*(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness)));
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f contrast -p1 0x%02X; cd $current_dir", VEYE_COMMANDS_FOLDER307, pModel->camera_params[iCameraIndex].profiles[iProfile].contrast);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f satu -p1 0x%02X; cd $current_dir", VEYE_COMMANDS_FOLDER307, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2));
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].shutterspeed != pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed == 0 )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 0; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         else
         {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 1; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         hw_execute_bash_command(szComm, NULL);

         char szShutter[24];
         snprintf(szShutter, sizeof(szShutter), "%d", (int)(1000000l/(long)pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed));
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f metime -p1 %s; cd $current_dir", VEYE_COMMANDS_FOLDER307, szShutter);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 1; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 0; cd $current_dir", VEYE_COMMANDS_FOLDER307);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 3; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 0; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   else // IMX 327 camera
   {
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      if ( bApplyAll )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f  videofmt -p1 NTSC -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFF -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFE -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f brightness -p1 0x%02X -b %d 2>&1; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness), nBus);
         char szOutput[1024];
         szOutput[0] = 0;
         hw_execute_bash_command_raw(szComm, szOutput);
         log_line("output: [%s]", szOutput);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(2.5*pModel->camera_params[iCameraIndex].profiles[iProfile].contrast), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f saturation -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].wdr != pModel->camera_params[iCameraIndex].profiles[iProfile].wdr )
      {
         int wdrmode = (int) pModel->camera_params[iCameraIndex].profiles[iProfile].wdr;
         if ( wdrmode < 0 || wdrmode > 3 )
            wdrmode = 0;
         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wdrmode -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, wdrmode, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].shutterspeed != pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed == 0 )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 0x40 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
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

         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x1B -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x18 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].sharpness != pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness >= 100 )
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness <= 110 )
         {
            if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness == 100 )
               snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x0 -p2 0x0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
            else
               snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x1 -p2 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness - 100, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].drc != pModel->camera_params[iCameraIndex].profiles[iProfile].drc )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].drc<16 )
         {
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f agc -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].drc, nBus );
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x03 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x00 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   memcpy((u8*)&s_LastAppliedVeyeCameraParams, (u8*)&(pModel->camera_params[iCameraIndex]), sizeof(type_camera_parameters));
   memcpy((u8*)&s_LastAppliedVeyeVideoParams, (u8*)&(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile]), sizeof(type_video_link_profile));
}


void vehicle_launch_audio_capture(Model* pModel)
{
   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) || (! pModel->audio_params.enabled) )
      return;
   char szComm[128];
   char szRate[32];
   snprintf(szComm, sizeof(szComm), "amixer -c 1 sset Mic %d%%", pModel->audio_params.volume);
   hw_execute_bash_command(szComm, NULL);

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
   snprintf(szComm, sizeof(szComm), "arecord --device=hw:1,0 --file-type wav --format S16_LE --rate %s -c 1 >> %s &", szRate, FIFO_RUBY_AUDIO1);
   hw_execute_bash_command(szComm, NULL);
}

void vehicle_stop_audio_capture(Model* pModel)
{
   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) )
      return;

   //hw_execute_bash_command("kill -9 $(pidof arecord) 2>/dev/null", NULL);
   hw_stop_process("arecord");
}

void vehicle_check_update_processes_affinities()
{
   char szOutput[128];

   hw_execute_bash_command_raw("nproc --all", szOutput);
   int iCores = 0;
   if ( 1 != sscanf(szOutput, "%d", &iCores) )
   {
      log_softerror_and_alarm("Failed to get CPU cores count. No affinity adjustments for processes to be done.");
      return;    
   }

   if ( iCores < 2 || iCores > 32 )
   {
      log_line("Single core CPU, no affinity adjustments for processes to be done.");
      return;
   }
   log_line("%d CPU cores, doing affinity adjustments for processes...", iCores);
   if ( iCores > 2 )
   {
      hw_set_proc_affinity("ruby_rt_vehicle", 1,1);
      hw_set_proc_affinity("ruby_tx_telemetry", 2,2);
      hw_set_proc_affinity("ruby_rx_rc", 2,2);
      hw_set_proc_affinity("ruby_capture_raspi", 3, iCores);
   }
   else
   {
      hw_set_proc_affinity("ruby_rt_vehicle", 1,1);
      hw_set_proc_affinity("ruby_capture_raspi", 2, iCores);
   }
}
