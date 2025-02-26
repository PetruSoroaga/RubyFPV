/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_confirmation.h"
#include "menu_controller_expert.h"
#include "menu_item_section.h"
#include "../process_router_messages.h"
#include "../../base/hardware_files.h"
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuControllerExpert::MenuControllerExpert(void)
:Menu(MENU_ID_CONTROLLER_EXPERT, "CPU and Processes Settings", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;
   
   float fSliderWidth = 0.12;

   readConfigFile();
   addTopInfo();

   m_IndexEnableRadioThreadsPriority = -1;
   m_IndexRadioRxPriority = -1;
   m_IndexRadioTxPriority = -1;

   m_iIndexCoresAdjustment = -1;
   m_iIndexPrioritiesAdjustment = -1;

   m_IndexCPUEnabled = -1;
   m_IndexCPUSpeed = -1;
   m_IndexGPUEnabled = -1;
   m_IndexGPUSpeed = -1;
   m_IndexVoltageEnabled = -1;
   m_IndexVoltage = -1;
   m_IndexReset = -1;

   #if defined(HW_PLATFORM_RASPBERRY)
   addMenuItem(new MenuItemSection("CPU"));

   m_pItemsSelect[2] = new MenuItemSelect("Enable CPU Overclocking", "Enables overclocking of the main ARM CPU.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexCPUEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[5] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency. Requires a reboot.", 700,1600, 1050, fSliderWidth);
   m_pItemsSlider[5]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSelect[3] = new MenuItemSelect("Enable GPU Overclocking", "Enables overclocking of the GPU cores.");  
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setIsEditable();
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
   #endif

   addMenuItem(new MenuItemSection("Priorities"));

   m_pItemsSelect[10] = new MenuItemSelect("Enable CPU Cores Auto Adjustment", "Automatically adjust the work load on each CPU core.");
   m_pItemsSelect[10]->addSelection("No");
   m_pItemsSelect[10]->addSelection("Yes");
   m_pItemsSelect[10]->setIsEditable();
   m_iIndexCoresAdjustment = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[11] = new MenuItemSelect("Enable Priorities Adjustment", "Enable adjustment of processes priorities or use default priorities.");
   m_pItemsSelect[11]->addSelection("No");
   m_pItemsSelect[11]->addSelection("Yes");
   m_pItemsSelect[11]->setIsEditable();
   m_iIndexPrioritiesAdjustment = addMenuItem(m_pItemsSelect[11]);

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

   m_pItemsSelect[5] = new MenuItemSelect("Video Priority", "Sets a manual CPU and IO priority for the video renderer process.");  
   m_pItemsSelect[5]->addSelection("Auto");
   m_pItemsSelect[5]->addSelection("Manual");
   m_pItemsSelect[5]->setIsEditable();
   m_IndexAutoRxVideo = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSlider[2] = new MenuItemSlider("   Video CPU Priority",  "Sets the priority for the video RX pipeline. Higher values means higher priority.", 0,15,9, fSliderWidth);
   m_IndexNiceRXVideo = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSelect[1] = new MenuItemSelect("   Video I/O Boost", "Sets a higher priority for the I/O operations and data flows for the received video stream. Other components might work slower.");  
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexIONiceRXVideo = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSlider[3] = new MenuItemSlider("   Video I/O Priority", "Sets the I/O priority value for RX video pipeline, 1 is highest priority, 7 is lowest priority.", 1,7,4, fSliderWidth);
   m_pItemsSlider[3]->setStep(1);
   m_IndexIONiceValueRXVideo = addMenuItem(m_pItemsSlider[3]);


   m_pItemsSlider[4] = new MenuItemSlider("Ruby UI/OSD priority", "Sets the priority for the Ruby UI process. Higher values means higher priority.", 0,15,5, fSliderWidth);
   m_IndexNiceCentral = addMenuItem(m_pItemsSlider[4]);


   m_pItemsSelect[8] = new MenuItemSelect("Radio Threads Adjustment", "Change the way the priority of Ruby radio threads is adjusted.");
   m_pItemsSelect[8]->addSelection("Default");
   m_pItemsSelect[8]->addSelection("Custom");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexEnableRadioThreadsPriority = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSlider[8] = new MenuItemSlider("   Rx Threads Priority", "Sets the priority for the Ruby radio Rx threads. Higher values means higher priority.", 0,90,10, fSliderWidth);
   m_IndexRadioRxPriority = addMenuItem(m_pItemsSlider[8]);

   m_pItemsSlider[9] = new MenuItemSlider("   Tx Threads Priority", "Sets the priority for the Ruby radio Rx threads. Higher values means higher priority.", 0,90,10, fSliderWidth);
   m_IndexRadioTxPriority = addMenuItem(m_pItemsSlider[9]);


   m_IndexVersions = addMenuItem(new MenuItem("Modules versions", "Get all modules versions."));
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

   m_pItemsSelect[10]->setSelectedIndex(pcs->iCoresAdjustment);
   m_pItemsSelect[11]->setSelectedIndex(pcs->iPrioritiesAdjustment);
   
   m_pItemsSlider[0]->setCurrentValue(-pcs->iNiceRouter);
   m_pItemsSlider[4]->setCurrentValue(-pcs->iNiceCentral);

   m_pItemsSelect[0]->setSelection(pcs->ioNiceRouter > 0);
   if ( pcs->ioNiceRouter > 0 )
      m_pItemsSlider[1]->setCurrentValue( pcs->ioNiceRouter);
   else
      m_pItemsSlider[1]->setCurrentValue(-pcs->ioNiceRouter);
   m_pItemsSlider[1]->setEnabled( pcs->ioNiceRouter > 0);

   if ( pcs->iNiceRXVideo < 0 )
   {
      m_pItemsSelect[5]->setSelectedIndex(1);
      m_pItemsSelect[1]->setEnabled(true);
      m_pItemsSlider[2]->setEnabled(true);
      m_pItemsSlider[3]->setEnabled(true);

      m_pItemsSlider[2]->setCurrentValue(-pcs->iNiceRXVideo);
      m_pItemsSelect[1]->setSelection(pcs->ioNiceRXVideo > 0);
      if ( pcs->ioNiceRXVideo > 0 )
         m_pItemsSlider[3]->setCurrentValue( pcs->ioNiceRXVideo);
      else
         m_pItemsSlider[3]->setCurrentValue(-pcs->ioNiceRXVideo);
      m_pItemsSlider[3]->setEnabled( pcs->ioNiceRXVideo > 0);
   }
   else
   {
      m_pItemsSelect[5]->setSelectedIndex(0);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSlider[2]->setEnabled(false);
      m_pItemsSlider[3]->setEnabled(false);
   }

   // Radio Threads

   if ( (pcs->iRadioRxThreadPriority <= 0) || (pcs->iRadioTxThreadPriority <= 0) )
   {
      m_pItemsSelect[8]->setSelectedIndex(0);

      m_pItemsSlider[8]->setEnabled(false);
      m_pItemsSlider[9]->setEnabled(false);
      m_pItemsSlider[9]->setCurrentValue(0);
      m_pItemsSlider[8]->setCurrentValue(0);
   }
   else
   {
      m_pItemsSelect[8]->setSelectedIndex(1);
      if ( pcs->iRadioRxThreadPriority < 1 )
         pcs->iRadioRxThreadPriority = 1;
      if ( pcs->iRadioTxThreadPriority < 1 )
         pcs->iRadioTxThreadPriority = 1;
      m_pItemsSlider[8]->setCurrentValue(pcs->iRadioRxThreadPriority);
      m_pItemsSlider[9]->setCurrentValue(pcs->iRadioTxThreadPriority);
      if ( ! pcs->iPrioritiesAdjustment )
      {
         m_pItemsSlider[9]->setCurrentValue(0);
         m_pItemsSlider[8]->setCurrentValue(0);       
      }
      m_pItemsSlider[8]->setEnabled(pcs->iPrioritiesAdjustment);
      m_pItemsSlider[9]->setEnabled(pcs->iPrioritiesAdjustment);

   }

   m_pItemsSlider[0]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSlider[1]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSlider[2]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSlider[3]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSlider[4]->setEnabled(pcs->iPrioritiesAdjustment);
   
   m_pItemsSelect[0]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSelect[1]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSelect[5]->setEnabled(pcs->iPrioritiesAdjustment);
   m_pItemsSelect[8]->setEnabled(pcs->iPrioritiesAdjustment);

   // CPU

   #if defined(HW_PLATFORM_RASPBERRY)
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
   #endif
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

   #if defined(HW_PLATFORM_RASPBERRY)
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
   #endif

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

   #if defined(HW_PLATFORM_RASPBERRY)
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
   #endif
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

   snprintf(szBuffer, sizeof(szBuffer)/sizeof(szBuffer[0]), "%s, %s CPU Cores, %s", szBoard, szOutput, szOutput2);
   addTopLine(szBuffer);

   #if defined(HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command_raw("vcgencmd measure_clock core", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "GPU %s Hz", szOutput);
   addTopLine(szBuffer);
   #endif

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

   #if defined(HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command_raw("vcgencmd measure_volts core", szOutput);
   sprintf(szBuffer, "CPU Voltage: %s", szOutput);
   addTopLine(szBuffer);
   ruby_signal_alive();

   char szTmp[256];

   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_c", szOutput);
   sprintf(szBuffer, "SDRAM C%s", szOutput);
   szBuffer[strlen(szBuffer)-1] = 0;
   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_i", szOutput);

   snprintf(szTmp, sizeof(szTmp)/sizeof(szTmp[0]), ", I%s", szOutput);
   strcat(szBuffer, szTmp);
   szBuffer[strlen(szBuffer)-1] = 0;
   ruby_signal_alive();

   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_p", szOutput);
   snprintf(szTmp, sizeof(szTmp)/sizeof(szTmp[0]), ", P%s", szOutput);
   strcat(szBuffer, szTmp);
   addTopLine(szBuffer);
   
   addTopLine(" ");
   addTopLine("Note: Changing overclocking settings requires a reboot.");
   addTopLine(" ");
   #endif
   
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

void MenuControllerExpert::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      onEventReboot();
      hardware_reboot();
   }
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

   if ( (-1 != m_iIndexCoresAdjustment) && (m_iIndexCoresAdjustment == m_SelectedIndex) )
   {
      if ( pcs->iCoresAdjustment != m_pItemsSelect[10]->getSelectedIndex() )
         addMessage(L("You must restart your controller for changes to take effect."));
      pcs->iCoresAdjustment = m_pItemsSelect[10]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( (-1 != m_iIndexPrioritiesAdjustment) && (m_iIndexPrioritiesAdjustment == m_SelectedIndex) )
   {
      if ( pcs->iPrioritiesAdjustment != m_pItemsSelect[11]->getSelectedIndex() )
         addMessage(L("You must restart your controller for changes to take effect."));
      pcs->iPrioritiesAdjustment = m_pItemsSelect[11]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
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
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "renice -n %d -p %s", pcs->iNiceRouter, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
   }

   if ( m_IndexAutoRxVideo == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         pcs->iNiceRXVideo = 0;
      else
         pcs->iNiceRXVideo = -10;
      valuesToUI();

      #if defined (HW_PLATFORM_RASPBERRY)
      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_SM);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "renice -n %d -p %s", pcs->iNiceRXVideo, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      #endif
   }

   if ( m_IndexNiceRXVideo == m_SelectedIndex )
   {
      pcs->iNiceRXVideo = -m_pItemsSlider[2]->getCurrentValue();
      #if defined (HW_PLATFORM_RASPBERRY)
      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_SM);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "renice -n %d -p %s", pcs->iNiceRXVideo, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      #endif
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

      #ifdef HW_CAPABILITY_IONICE
      char szBuff[1024];
      char szPids[1024];
      hw_execute_bash_command("pidof ruby_rt_station", szPids);
      if ( strlen(szPids) > 2 )
      {
         if ( pcs->ioNiceRouter > 0 )
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ionice -c 1 -n %d -p %s", pcs->ioNiceRouter, szPids);
         else
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ionice -c 2 -n 5 -p %s", szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      #endif
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

      #if defined (HW_PLATFORM_RASPBERRY)
      #ifdef HW_CAPABILITY_IONICE
      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_SM);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         if ( pcs->ioNiceRXVideo > 0 )
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ionice -c 1 -n %d -p %s", pcs->ioNiceRXVideo, szPids);
         else
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ionice -c 2 -n 5 -p %s", szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      #endif
      #endif
      valuesToUI();
   }

   if ( m_IndexIONiceRouterValue == m_SelectedIndex )
   {
      pcs->ioNiceRouter = m_pItemsSlider[1]->getCurrentValue();
      #ifdef HW_CAPABILITY_IONICE
      char szBuff[1024];
      char szPids[1024];
      hw_execute_bash_command("pidof ruby_rt_station", szPids);
      if ( strlen(szPids) > 2 )
      {
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ionice -c 1 -n %d -p %s", pcs->ioNiceRouter, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      #endif
   }


   if ( m_IndexIONiceValueRXVideo == m_SelectedIndex )
   {
      pcs->ioNiceRXVideo = m_pItemsSlider[3]->getCurrentValue();
      #if defined (HW_PLATFORM_RASPBERRY)
      #ifdef HW_CAPABILITY_IONICE
      char szBuff[1024];
      char szPids[1024];
      sprintf(szBuff, "pidof %s", VIDEO_PLAYER_SM);
      hw_execute_bash_command(szBuff, szPids);
      if ( strlen(szPids) > 2 )
      {
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ionice -c 1 -n %d -p %s", pcs->ioNiceRXVideo, szPids);
         hw_execute_bash_command(szBuff, NULL);
      }
      #endif
      #endif
   }

   if ( m_IndexEnableRadioThreadsPriority == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
      {
         pcs->iRadioRxThreadPriority = -1;
         pcs->iRadioTxThreadPriority = -1;
      }
      else
      {
         pcs->iRadioRxThreadPriority = m_pItemsSlider[8]->getCurrentValue();
         pcs->iRadioTxThreadPriority = m_pItemsSlider[9]->getCurrentValue();
      }
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }
   if ( (m_IndexRadioRxPriority == m_SelectedIndex) || (m_IndexRadioTxPriority == m_SelectedIndex) )
   {
      pcs->iRadioRxThreadPriority = m_pItemsSlider[8]->getCurrentValue();
      pcs->iRadioTxThreadPriority = m_pItemsSlider[9]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      log_line("MenuControllerCPU: New Radio Rx/Tx priorities: %d/%d", pcs->iRadioRxThreadPriority, pcs->iRadioTxThreadPriority);
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         char szText[256];
         if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
            sprintf(szText, "Your %s is armed. Are you sure you want to reboot the controller?", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         else
            strcpy(szText, "Your vehicle is armed. Are you sure you want to reboot the controller?");
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation", szText, 1);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }

      onEventReboot();
      hardware_reboot();
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

   if ( m_IndexVersions == m_SelectedIndex )
   {
      char szComm[256];
      char szBuff[1024];
      char szOutput[1024];

      Menu* pMenu = new Menu(0,"All Modules Versions",NULL);
      pMenu->m_xPos = 0.32;
      pMenu->m_yPos = 0.17;
      pMenu->m_Width = 0.6;
      
      hw_execute_bash_command_raw_silent("./ruby_start -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_start: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_controller -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_controller: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_central -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_central: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rt_station -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_rt_station: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rx_telemetry -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_rx_telemetry: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_tx_rc -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_tx_rc: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_i2c -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_i2c: %s", szOutput);
      pMenu->addTopLine(szBuff);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -ver", VIDEO_PLAYER_PIPE);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: %s", VIDEO_PLAYER_PIPE, szOutput);
      pMenu->addTopLine(szBuff);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -ver", VIDEO_PLAYER_SM);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: %s", VIDEO_PLAYER_SM, szOutput);
      pMenu->addTopLine(szBuff);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -ver", VIDEO_PLAYER_OFFLINE);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: %s", VIDEO_PLAYER_OFFLINE, szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_update_worker -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_update_worker: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rt_vehicle -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_rt_vehicle: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_tx_telemetry -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_tx_telemetry: %s", szOutput);
      pMenu->addTopLine(szBuff);

      add_menu_to_stack(pMenu);
      return;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      u32 board_type = hardware_getBoardType() & BOARD_TYPE_MASK;
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
