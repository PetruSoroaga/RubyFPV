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

#include "menu.h"
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_confirmation.h"
#include "menu_controller_expert.h"
#include "menu_item_section.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuControllerExpert::MenuControllerExpert(void)
:Menu(MENU_ID_CONTROLLER_EXPERT, "CPU and Processes Settings", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;
   m_ExtraItemsHeight = 0.0;

   float fSliderWidth = 0.12;

   m_iConfirmationId = 0;
   readConfigFile();
   addTopInfo();

   addMenuItem(new MenuItemSection("Priorities"));

   m_pItemsSlider[0] = new MenuItemSlider("Core Priority",  "Sets the priority for the Ruby core functionality. Higher values means higher priority.", 0,18,11, fSliderWidth);
   m_IndexNiceRouter = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[0] = new MenuItemSelect("Core I/O Boost", "Sets a higher priority for the I/O operations and data flows for the core Ruby components. Other components might work slower.");  
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexIONiceRouter = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[1] = new MenuItemSlider("Core I/O Priority", "Sets the I/O priority value for the core Ruby components, 1 is highest priority, 7 is lowest priority.", 1,7,4, fSliderWidth);
   m_pItemsSlider[1]->setStep(1);
   m_IndexIONiceRouterValue = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Video Priority",  "Sets the priority for the video RX pipeline. Higher values means higher priority.", 0,15,9, fSliderWidth);
   m_IndexNiceRXVideo = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSelect[1] = new MenuItemSelect("Video I/O Boost", "Sets a higher priority for the I/O operations and data flows for the received video stream. Other components might work slower.");  
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexIONiceRXVideo = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSlider[3] = new MenuItemSlider("Video I/O Priority", "Sets the I/O priority value for RX video pipeline, 1 is highest priority, 7 is lowest priority.", 1,7,4, fSliderWidth);
   m_pItemsSlider[3]->setStep(1);
   m_IndexIONiceValueRXVideo = addMenuItem(m_pItemsSlider[3]);


   m_pItemsSlider[4] = new MenuItemSlider("Ruby UI/OSD priority", "Sets the priority for the Ruby UI process. Higher values means higher priority.", 0,15,5, fSliderWidth);
   m_IndexNiceCentral = addMenuItem(m_pItemsSlider[4]);


   addMenuItem(new MenuItemSection("CPU"));

   m_pItemsSelect[2] = new MenuItemSelect("Enable CPU Overclocking", "Enables overclocking of the main ARM CPU.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_IndexCPUEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[5] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency. Requires a reboot.", 700,1600, 1050, fSliderWidth);
   m_pItemsSlider[5]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSelect[3] = new MenuItemSelect("Enable GPU Overclocking", "Enables overclocking of the GPU cores.");  
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_IndexGPUEnabled = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[6] = new MenuItemSlider("GPU Speed (Mhz)", "Sets the GPU frequency. Requires a reboot.", 200,1000,600, fSliderWidth);
   m_pItemsSlider[6]->setStep(25);
   m_IndexGPUSpeed = addMenuItem(m_pItemsSlider[6]);

   m_pItemsSelect[4] = new MenuItemSelect("Enable Overvoltage", "Enables overvotage on the CPU and GPU cores. You need to increase voltage as you increase speed.");  
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_IndexVoltageEnabled = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSlider[7] = new MenuItemSlider("Overvoltage (steps)", "Sets the overvoltage value, in 0.025V increments. Requires a reboot.", 1,8,4, fSliderWidth);
   m_pItemsSlider[7]->setStep(1);
   m_IndexVoltage = addMenuItem(m_pItemsSlider[7]);

   m_IndexReset = addMenuItem(new MenuItem("Reset CPU Freq", "Resets the controller CPU and GPU frequencies to default values."));

   m_IndexReboot = addMenuItem(new MenuItem("Restart", "Restarts the controller."));
}

void MenuControllerExpert::valuesToUI()
{
   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   m_pItemsSlider[0]->setCurrentValue(-pcs->iNiceRouter);
   m_pItemsSlider[2]->setCurrentValue(-pcs->iNiceRXVideo);
   m_pItemsSlider[4]->setCurrentValue(-pcs->iNiceCentral);

   m_pItemsSelect[0]->setSelection(pcs->ioNiceRouter > 0);
   if ( pcs->ioNiceRouter > 0 )
      m_pItemsSlider[1]->setCurrentValue( pcs->ioNiceRouter);
   else
      m_pItemsSlider[1]->setCurrentValue(-pcs->ioNiceRouter);
   m_pItemsSlider[1]->setEnabled( pcs->ioNiceRouter > 0);

   m_pItemsSelect[1]->setSelection(pcs->ioNiceRXVideo > 0);
   if ( pcs->ioNiceRXVideo > 0 )
      m_pItemsSlider[3]->setCurrentValue( pcs->ioNiceRXVideo);
   else
      m_pItemsSlider[3]->setCurrentValue(-pcs->ioNiceRXVideo);
   m_pItemsSlider[3]->setEnabled( pcs->ioNiceRXVideo > 0);

   // CPU

   m_pItemsSelect[2]->setSelection(pcs->iFreqARM > 0);
   if ( pcs->iFreqARM <= 0 )
      m_pItemsSlider[5]->setCurrentValue(m_iDefaultARMFreq);
   else
      m_pItemsSlider[5]->setCurrentValue(pcs->iFreqARM);
   m_pItemsSlider[5]->setEnabled(pcs->iFreqARM > 0);

   m_pItemsSelect[3]->setSelection(pcs->iFreqGPU > 0);
   if ( pcs->iFreqGPU <= 0 )
      m_pItemsSlider[6]->setCurrentValue(m_iDefaultGPUFreq);
   else
      m_pItemsSlider[6]->setCurrentValue(pcs->iFreqGPU);
   m_pItemsSlider[6]->setEnabled(pcs->iFreqGPU > 0);

   m_pItemsSelect[4]->setSelection(pcs->iOverVoltage > 0);
   if ( pcs->iOverVoltage <= 0 )
      m_pItemsSlider[7]->setCurrentValue(m_iDefaultVoltage);
   else
      m_pItemsSlider[7]->setCurrentValue(pcs->iOverVoltage);
   m_pItemsSlider[7]->setEnabled(pcs->iOverVoltage > 0);
}


void MenuControllerExpert::readConfigFile()
{
   ruby_signal_alive();

   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   ruby_signal_alive();

   m_iDefaultARMFreq = config_file_get_value("arm_freq");
   if ( m_iDefaultARMFreq < 0 )
      m_iDefaultARMFreq = - m_iDefaultARMFreq;

   m_iDefaultGPUFreq = config_file_get_value("gpu_freq");
   if ( m_iDefaultGPUFreq < 0 )
      m_iDefaultGPUFreq = - m_iDefaultGPUFreq;

   m_iDefaultVoltage = config_file_get_value("over_voltage");
   if ( m_iDefaultVoltage < 0 )
      m_iDefaultVoltage = - m_iDefaultVoltage;

   pcs->iFreqARM = config_file_get_value("arm_freq");
   ruby_signal_alive();
   pcs->iFreqGPU = config_file_get_value("gpu_freq");
   ruby_signal_alive();
   pcs->iOverVoltage = config_file_get_value("over_voltage");
   ruby_signal_alive();

   save_ControllerSettings();
   ruby_signal_alive();
}

void MenuControllerExpert::writeConfigFile()
{
   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   ruby_signal_alive();
   hardware_mount_boot();
   hardware_sleep_ms(50);
   ruby_signal_alive();
   hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

   config_file_set_value("config.txt", "over_voltage", pcs->iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_sdram", pcs->iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_min", pcs->iOverVoltage);
   ruby_signal_alive();

   config_file_set_value("config.txt", "arm_freq", pcs->iFreqARM);
   config_file_set_value("config.txt", "arm_freq_min", pcs->iFreqARM);
   ruby_signal_alive();

   config_file_set_value("config.txt", "gpu_freq", pcs->iFreqGPU);
   config_file_set_value("config.txt", "gpu_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "core_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "h264_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "isp_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "v3d_freq_min", pcs->iFreqGPU);
   ruby_signal_alive();

   config_file_set_value("config.txt", "sdram_freq", pcs->iFreqGPU);
   config_file_set_value("config.txt", "sdram_freq_min", pcs->iFreqGPU);
   ruby_signal_alive();

   hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
   ruby_signal_alive();
}

void MenuControllerExpert::addTopInfo()
{
   char szBuffer[2048];
   char szOutput[1024];
   char szOutput2[1024];

   log_line("Menu Controller Expert: adding info...");

   int board_type = hardware_getBoardType();
   const char* szBoard = str_get_hardware_board_name(board_type);
   ruby_signal_alive();

   hw_execute_bash_command_raw("nproc --all", szOutput);
   szOutput[strlen(szOutput)-1] = 0;

   int speed = hardware_get_cpu_speed();
   sprintf(szOutput2, "%d Mhz", speed);

   sprintf(szBuffer, "%s, %s CPU Cores, %s Hz", szBoard, szOutput, szOutput2);
   addTopLine(szBuffer);

   hw_execute_bash_command_raw("vcgencmd measure_clock core", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "GPU %s Hz", szOutput);
   addTopLine(szBuffer);

   /*
   hw_execute_bash_command_raw("vcgencmd measure_clock h264", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "H264 %s Hz", szOutput);
   addTopLine(szBuffer);

   hw_execute_bash_command_raw("vcgencmd measure_clock isp", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "ISP %s Hz", szOutput);
   addTopLine(szBuffer);
   */

   hw_execute_bash_command_raw("vcgencmd measure_volts core", szOutput);
   sprintf(szBuffer, "CPU Voltage: %s", szOutput);
   addTopLine(szBuffer);
   ruby_signal_alive();

   char szTmp[256];

   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_c", szOutput);
   sprintf(szBuffer, "SDRAM C%s", szOutput);
   szBuffer[strlen(szBuffer)-1] = 0;
   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_i", szOutput);

   sprintf(szTmp, ", I%s", szOutput);
   strcat(szBuffer, szTmp);
   szBuffer[strlen(szBuffer)-1] = 0;
   ruby_signal_alive();

   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_p", szOutput);
   sprintf(szTmp, ", P%s", szOutput);
   strcat(szBuffer, szTmp);
   addTopLine(szBuffer);

   addTopLine(" ");
   addTopLine("Note: Changing overclocking settings requires a reboot.");
   addTopLine(" ");

   log_line("Menu Controller Expert: added info.");
}


void MenuControllerExpert::onShow()
{
   Menu::onShow();
}

void MenuControllerExpert::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerExpert::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 10 == m_iConfirmationId && 1 == returnValue )
   {
      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }
   m_iConfirmationId = 0;
}


void MenuControllerExpert::onSelectItem()
{
   Menu::onSelectItem();
   
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   if ( m_IndexNiceRouter == m_SelectedIndex )
   {
      pcs->iNiceRouter = -m_pItemsSlider[0]->getCurrentValue();
      char szBuff[1024];
      char szPids[1024];
      hw_execute_bash_command("pidof ruby_rt_station", szPids);
      if ( strlen(szPids) > 2 )
      {
         sprintf(szBuff, "renice -n %d -p %s", pcs->iNiceRouter, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
   }

   if ( m_IndexNiceRXVideo == m_SelectedIndex )
   {
      pcs->iNiceRXVideo = -m_pItemsSlider[2]->getCurrentValue();
      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_PIPE);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         sprintf(szBuff, "renice -n %d -p %s", pcs->iNiceRXVideo, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
   }

   if ( m_IndexNiceCentral == m_SelectedIndex )
   {
      pcs->iNiceCentral = -m_pItemsSlider[4]->getCurrentValue();     
      log_line("Set Ruby UI/OSD Central nice priority to: %d", pcs->iNiceCentral); 
      hw_set_priority_current_proc(pcs->iNiceCentral); 
   }


   if ( m_IndexIONiceRouter == m_SelectedIndex )
   {
      int ioNice = pcs->ioNiceRouter;
      if ( ioNice < 0 )
         ioNice = -ioNice;
      if ( ioNice == 0 )
         ioNice = 5;

      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         ioNice = -ioNice;

      pcs->ioNiceRouter = ioNice;

      char szBuff[1024];
      char szPids[1024];
      hw_execute_bash_command("pidof ruby_rt_station", szPids);
      if ( strlen(szPids) > 2 )
      {
         if ( pcs->ioNiceRouter > 0 )
            sprintf(szBuff, "ionice -c 1 -n %d -p %s", pcs->ioNiceRouter, szPids);
         else
            sprintf(szBuff, "ionice -c 2 -n 5 -p %s", szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      valuesToUI();
   }

   if ( m_IndexIONiceRXVideo == m_SelectedIndex )
   {
      int ioNice = pcs->ioNiceRXVideo;
      if ( ioNice < 0 )
         ioNice = -ioNice;
      if ( ioNice == 0 )
         ioNice = 5;

      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         ioNice = -ioNice;

      pcs->ioNiceRXVideo = ioNice;

      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_PIPE);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         if ( pcs->ioNiceRXVideo > 0 )
            sprintf(szBuff, "ionice -c 1 -n %d -p %s", pcs->ioNiceRXVideo, szPids);
         else
            sprintf(szBuff, "ionice -c 2 -n 5 -p %s", szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      valuesToUI();
   }

   if ( m_IndexIONiceRouterValue == m_SelectedIndex )
   {
      pcs->ioNiceRouter = m_pItemsSlider[1]->getCurrentValue();
      char szBuff[1024];
      char szPids[1024];
      hw_execute_bash_command("pidof ruby_rt_station", szPids);
      if ( strlen(szPids) > 2 )
      {
         sprintf(szBuff, "ionice -c 1 -n %d -p %s", pcs->ioNiceRouter, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
   }


   if ( m_IndexIONiceValueRXVideo == m_SelectedIndex )
   {
      pcs->ioNiceRXVideo = m_pItemsSlider[3]->getCurrentValue();
      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_PIPE);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         sprintf(szBuff, "ionice -c 1 -n %d -p %s", pcs->ioNiceRXVideo, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         m_iConfirmationId = 10;
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation","Your vehicle is armed. Are you sure you want to reboot the controller?", m_iConfirmationId);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }

      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   bool bUpdatedConfig = false;

   if ( m_IndexCPUEnabled == m_SelectedIndex )
   {
      if ( m_iDefaultARMFreq <= 0 )
         m_iDefaultARMFreq = 900;
      if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
         pcs->iFreqARM = m_iDefaultARMFreq;
      else
         pcs->iFreqARM = -m_iDefaultARMFreq;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexGPUEnabled == m_SelectedIndex )
   {
      if ( m_iDefaultGPUFreq <= 0 )
         m_iDefaultGPUFreq = 400;

      if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         pcs->iFreqGPU = m_iDefaultGPUFreq;
      else
         pcs->iFreqGPU = -m_iDefaultGPUFreq;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexVoltageEnabled == m_SelectedIndex )
   {
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         pcs->iOverVoltage = m_iDefaultVoltage;
      else
         pcs->iOverVoltage = -m_iDefaultVoltage;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexCPUSpeed == m_SelectedIndex )
   {
      pcs->iFreqARM = m_pItemsSlider[5]->getCurrentValue();
      m_iDefaultARMFreq = pcs->iFreqARM;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexGPUSpeed == m_SelectedIndex )
   {
      pcs->iFreqGPU = m_pItemsSlider[6]->getCurrentValue();
      m_iDefaultGPUFreq = pcs->iFreqGPU;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexVoltage == m_SelectedIndex )
   {
      pcs->iOverVoltage = m_pItemsSlider[7]->getCurrentValue();
      m_iDefaultVoltage = pcs->iOverVoltage;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      int board_type = hardware_getBoardType();
      pcs->iFreqARM = 900;
      if ( board_type == BOARD_TYPE_PIZERO2 )
         pcs->iFreqARM = 1000;
      else if ( board_type == BOARD_TYPE_PI3B )
         pcs->iFreqARM = 1200;
      else if ( board_type == BOARD_TYPE_PI3BPLUS || board_type == BOARD_TYPE_PI4B )
         pcs->iFreqARM = 1400;
      else if ( board_type != BOARD_TYPE_PIZERO && board_type != BOARD_TYPE_PIZEROW && board_type != BOARD_TYPE_NONE 
               && board_type != BOARD_TYPE_PI2B && board_type != BOARD_TYPE_PI2BV11 && board_type != BOARD_TYPE_PI2BV12 )
         pcs->iFreqARM = 1200;

      pcs->iFreqGPU = 400;
      pcs->iOverVoltage = 3;
      m_iDefaultARMFreq = pcs->iFreqARM;
      m_iDefaultGPUFreq = pcs->iFreqGPU;
      m_iDefaultVoltage = pcs->iOverVoltage;
      valuesToUI();
      bUpdatedConfig = true;
   }
   save_ControllerSettings();
   if ( bUpdatedConfig )
      writeConfigFile();
}
