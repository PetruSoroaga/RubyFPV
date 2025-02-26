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
#include "menu_vehicle_expert.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"

MenuVehicleExpert::MenuVehicleExpert(void)
:Menu(MENU_ID_VEHICLE_EXPERT, "Expert Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;
   float fSliderWidth = 0.10;
   setSubTitle("Change advanced vehicle settings, for expert users.");
   
   addTopInfo();

   addMenuItem(new MenuItemSection("Processes Priorities"));

   m_pItemsSlider[0] = new MenuItemSlider("Core Priority", "Sets the priority for the Ruby core functionality. Higher values means higher priority.", 0,18,11, fSliderWidth);
   m_IndexNiceRouter = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[0] = new MenuItemSelect("Core I/O Boost", "Sets a higher priority for the I/O operations and data flows for the core Ruby components. Other components might work slower.");  
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexIONiceRouter = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[1] = new MenuItemSlider("Core I/O Priority", "Sets the I/O priority value, 1 is highest priority, 7 is lowest priority.", 1,7,4, fSliderWidth);
   m_pItemsSlider[1]->setStep(1);
   m_IndexIONiceRouterValue = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Video Module Priority", "Sets the priority for the video TX/RX pipeline. Higher values means higher priority.", 0,15,9, fSliderWidth);
   m_IndexNiceVideo = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSelect[1] = new MenuItemSelect("Video I/O Boost", "Sets a higher priority for the I/O operations and data flows for the video stream. Other components might work slower.");  
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexIONice = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSlider[3] = new MenuItemSlider("Video I/O Priority", "Sets the I/O priority value, 1 is highest priority, 7 is lowest priority.", 1,7,4, fSliderWidth);
   m_pItemsSlider[3]->setStep(1);
   m_IndexIONiceValue = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSlider[4] = new MenuItemSlider("Telemetry Module Priority",  "Sets the priority for the telemetry module. Higher values means higher priority.", 0,16, 5, fSliderWidth);
   m_IndexNiceTelemetry = addMenuItem(m_pItemsSlider[4]);

   m_pItemsSlider[5] = new MenuItemSlider("RC Module Priority",  "Sets the priority for the RC module. Higher values means higher priority.", 0,17, 9, fSliderWidth);
   m_IndexNiceRC = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSlider[6] = new MenuItemSlider("Other Modules Priority",  "Sets the priority for the other modules. Higher values means higher priority.", 0,15, 2, fSliderWidth);
   m_IndexNiceOthers = addMenuItem(m_pItemsSlider[6]);

   addMenuItem(new MenuItemSection("Threads Priorities"));

   m_pItemsSelect[11] = new MenuItemSelect("Core Threads Adjustment", "Change the way the priority of Ruby threads is adjusted.");
   m_pItemsSelect[11]->addSelection("Auto");
   m_pItemsSelect[11]->addSelection("Custom");
   m_pItemsSelect[11]->setIsEditable();
   m_IndexEnableRouter = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSlider[11] = new MenuItemSlider("   Threads Priority", "Sets the priority for the Ruby threads. Higher values means higher priority.", 1,90,10, fSliderWidth);
   m_IndexRouter = addMenuItem(m_pItemsSlider[11]);

   m_pItemsSelect[12] = new MenuItemSelect("Radio Threads Adjustment", "Change the way the priority of Ruby radio threads is adjusted.");
   m_pItemsSelect[12]->addSelection("Auto");
   m_pItemsSelect[12]->addSelection("Custom");
   m_pItemsSelect[12]->setIsEditable();
   m_IndexEnableRadio = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSlider[12] = new MenuItemSlider("   Rx Threads Priority", "Sets the priority for the Ruby radio Rx threads. Higher values means higher priority.", 1,90,10, fSliderWidth);
   m_IndexRadioRx = addMenuItem(m_pItemsSlider[12]);

   m_pItemsSlider[13] = new MenuItemSlider("   Tx Threads Priority", "Sets the priority for the Ruby radio Rx threads. Higher values means higher priority.", 1,90,10, fSliderWidth);
   m_IndexRadioTx = addMenuItem(m_pItemsSlider[13]);

   addMenuItem(new MenuItemSection("Overclocking"));

   m_pItemsSelect[2] = new MenuItemSelect("Enable CPU Overclocking", "Enables overclocking of the main ARM CPU.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexCPUEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[7] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency. Requires a reboot.", 700,1600, 1050, fSliderWidth);
   m_pItemsSlider[7]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[7]);

   m_pItemsSelect[3] = new MenuItemSelect("Enable GPU Overclocking", "Enables overclocking of the GPU cores.");  
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexGPUEnabled = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[8] = new MenuItemSlider("GPU Speed (Mhz)", "Sets the GPU frequency. Requires a reboot.", 200,1000,600, fSliderWidth);
   m_pItemsSlider[8]->setStep(25);
   m_IndexGPUSpeed = addMenuItem(m_pItemsSlider[8]);

   m_pItemsSelect[4] = new MenuItemSelect("Enable Overvoltage", "Enables overvotage on the CPU and GPU cores. You need to increase voltage as you increase the frequency of the CPU or GPU.");  
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_IndexVoltageEnabled = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSlider[9] = new MenuItemSlider("Overvoltage (steps)", "Sets the overvoltage value, in 0.025V increments. Requires a reboot.", 1,8,4, fSliderWidth);
   m_pItemsSlider[9]->setStep(1);
   m_IndexVoltage = addMenuItem(m_pItemsSlider[9]);

   addMenuItem(new MenuItemSection("Other Settings"));

   m_pItemsSelect[5] = new MenuItemSelect("Enable eth and DHCP", "Enables local network and DHCP on the vehicle.");  
   m_pItemsSelect[5]->addSelection("No");
   m_pItemsSelect[5]->addSelection("Yes");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexDHCP = addMenuItem(m_pItemsSelect[5]);

   m_IndexReset = addMenuItem(new MenuItem("Reset CPU Freq", "Restarts the vehicle CPU and GPU frequencies to default values."));
   m_IndexReboot = addMenuItem(new MenuItem("Restart", "Restarts the vehicle."));
}

void MenuVehicleExpert::valuesToUI()
{
   m_pItemsSlider[0]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceRouter);
   m_pItemsSlider[2]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceVideo);
   m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceTelemetry);
   m_pItemsSlider[5]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceRC);
   m_pItemsSlider[6]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceOthers);

   log_line("menu rc nice: %d", g_pCurrentModel->processesPriorities.iNiceRC);
   m_pItemsSelect[0]->setSelection(g_pCurrentModel->processesPriorities.ioNiceRouter > 0);
   if ( g_pCurrentModel->processesPriorities.ioNiceRouter > 0 )
      m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->processesPriorities.ioNiceRouter);
   else
      m_pItemsSlider[1]->setCurrentValue(-g_pCurrentModel->processesPriorities.ioNiceRouter);
   m_pItemsSlider[1]->setEnabled(g_pCurrentModel->processesPriorities.ioNiceRouter > 0);

   m_pItemsSelect[1]->setSelection(g_pCurrentModel->processesPriorities.ioNiceVideo > 0);
   if ( g_pCurrentModel->processesPriorities.ioNiceVideo > 0 )
      m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->processesPriorities.ioNiceVideo);
   else
      m_pItemsSlider[3]->setCurrentValue(-g_pCurrentModel->processesPriorities.ioNiceVideo);
   m_pItemsSlider[3]->setEnabled(g_pCurrentModel->processesPriorities.ioNiceVideo > 0);

   // Threads

   if ( g_pCurrentModel->processesPriorities.iThreadPriorityRouter <= 0 )
   {
      m_pItemsSelect[11]->setSelectedIndex(0);
      m_pItemsSlider[11]->setCurrentValue(1);
      m_pItemsSlider[11]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[11]->setSelectedIndex(1);
      m_pItemsSlider[11]->setCurrentValue(g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
      m_pItemsSlider[11]->setEnabled(true);
   }

   if ( g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx == 0 )
   {
      m_pItemsSelect[12]->setSelectedIndex(0);
      m_pItemsSlider[12]->setCurrentValue(1);
      m_pItemsSlider[12]->setEnabled(false);
      m_pItemsSlider[13]->setCurrentValue(1);
      m_pItemsSlider[13]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[12]->setSelectedIndex(1);
      m_pItemsSlider[12]->setCurrentValue(g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx);
      m_pItemsSlider[12]->setEnabled(true);
      m_pItemsSlider[13]->setCurrentValue(g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx);
      m_pItemsSlider[13]->setEnabled(true);
   }

   // CPU

   m_pItemsSelect[2]->setSelection(g_pCurrentModel->processesPriorities.iFreqARM > 0);
   if ( g_pCurrentModel->processesPriorities.iFreqARM > 0 )
      m_pItemsSlider[7]->setCurrentValue(g_pCurrentModel->processesPriorities.iFreqARM);
   else
      m_pItemsSlider[7]->setCurrentValue(-g_pCurrentModel->processesPriorities.iFreqARM);
   m_pItemsSlider[7]->setEnabled(g_pCurrentModel->processesPriorities.iFreqARM > 0);

   m_pItemsSelect[3]->setSelection(g_pCurrentModel->processesPriorities.iFreqGPU > 0);
   if ( g_pCurrentModel->processesPriorities.iFreqGPU > 0 )
      m_pItemsSlider[8]->setCurrentValue(g_pCurrentModel->processesPriorities.iFreqGPU);
   else
      m_pItemsSlider[8]->setCurrentValue(-g_pCurrentModel->processesPriorities.iFreqGPU);
   m_pItemsSlider[8]->setEnabled(g_pCurrentModel->processesPriorities.iFreqGPU > 0);

   m_pItemsSelect[4]->setSelection(g_pCurrentModel->processesPriorities.iOverVoltage > 0);
   if ( g_pCurrentModel->processesPriorities.iOverVoltage > 0 )
      m_pItemsSlider[9]->setCurrentValue(g_pCurrentModel->processesPriorities.iOverVoltage);
   else
      m_pItemsSlider[9]->setCurrentValue(-g_pCurrentModel->processesPriorities.iOverVoltage);
   m_pItemsSlider[9]->setEnabled(g_pCurrentModel->processesPriorities.iOverVoltage > 0);   


   m_pItemsSelect[5]->setSelection(0);
   if ( g_pCurrentModel->enableDHCP )
      m_pItemsSelect[5]->setSelection(1);
}

void MenuVehicleExpert::addTopInfo()
{
   //char szBuffer[2048];
   //char szOutput[1024];
   //char szOutput2[1024];
      
   //addTopLine(" ");
   //addTopLine("Note: Changing overclocking settings requires a reboot.");
   //addTopLine(" ");
}

void MenuVehicleExpert::onShow()
{
   Menu::onShow();
}

void MenuVehicleExpert::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleExpert::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (10 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
      return;
   }
}

void MenuVehicleExpert::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      log_line("MenuCPU: Command in progress");
      handle_commands_show_popup_progress();
      return;
   }

   if ( ! menu_check_current_model_ok_for_edit() )
   {
      log_line("MenuCPU: Can't edit model.");
      return;
   }
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
   {
      log_line("MenuCPU: Item is still editing");
      return;
   }

   log_line("MenuCPU: Selected item %d", m_SelectedIndex);

   if ( m_IndexNiceRouter == m_SelectedIndex )
   {
      int nr = -m_pItemsSlider[0]->getCurrentValue();
      int nv = -m_pItemsSlider[2]->getCurrentValue();
      int nc = -m_pItemsSlider[5]->getCurrentValue();
      int no = -m_pItemsSlider[6]->getCurrentValue();
      u32 val = (nv+20) + (no+20)*256 + (nr+20)*256*256 + (nc+20)*256*256*256;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUES, val , NULL, 0) )
         valuesToUI();
      return;
   }


   if ( m_IndexNiceVideo == m_SelectedIndex )
   {
      int nr = -m_pItemsSlider[0]->getCurrentValue();
      int nv = -m_pItemsSlider[2]->getCurrentValue();
      int nc = -m_pItemsSlider[5]->getCurrentValue();
      int no = -m_pItemsSlider[6]->getCurrentValue();
      u32 val = (nv+20) + (no+20)*256 + (nr+20)*256*256 + (nc+20)*256*256*256;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUES, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexNiceRC == m_SelectedIndex )
   {
      int nr = -m_pItemsSlider[0]->getCurrentValue();
      int nv = -m_pItemsSlider[2]->getCurrentValue();
      int nc = -m_pItemsSlider[5]->getCurrentValue();
      int no = -m_pItemsSlider[6]->getCurrentValue();
      u32 val = (nv+20) + (no+20)*256 + (nr+20)*256*256 + (nc+20)*256*256*256;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUES, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexNiceOthers == m_SelectedIndex )
   {
      int nr = -m_pItemsSlider[0]->getCurrentValue();
      int nv = -m_pItemsSlider[2]->getCurrentValue();
      int nc = -m_pItemsSlider[5]->getCurrentValue();
      int no = -m_pItemsSlider[6]->getCurrentValue();
      u32 val = (nv+20) + (no+20)*256 + (nr+20)*256*256 + (nc+20)*256*256*256;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUES, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexNiceTelemetry == m_SelectedIndex )
   {
      int nt = -m_pItemsSlider[4]->getCurrentValue();
      u32 val = nt+20;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUE_TELEMETRY, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexIONiceRouter == m_SelectedIndex )
   {
      int ioNiceRouter = g_pCurrentModel->processesPriorities.ioNiceRouter;
      if ( ioNiceRouter < 0 )
         ioNiceRouter = -ioNiceRouter;
      if ( ioNiceRouter == 0 )
         ioNiceRouter = 5;

      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         ioNiceRouter = -ioNiceRouter;
      log_line("ionice router: %d, %d", ioNiceRouter, g_pCurrentModel->processesPriorities.ioNiceRouter);

      int ioNice = ((g_pCurrentModel->processesPriorities.ioNiceVideo+20) % 256);
      ioNice |= (((ioNiceRouter+20) % 256)<<8);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
      {
         valuesToUI();
      }
      return;
   }

   if ( m_IndexIONiceRouterValue == m_SelectedIndex )
   {
      int ioNice = ((g_pCurrentModel->processesPriorities.ioNiceVideo+20) % 256);
      ioNice |= (((m_pItemsSlider[1]->getCurrentValue()+20) % 256)<<8);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexIONice == m_SelectedIndex )
   {
      int ioNice = g_pCurrentModel->processesPriorities.ioNiceVideo;
      if ( ioNice < 0 )
         ioNice = -ioNice;
      if ( ioNice == 0 )
         ioNice = 5;

      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         ioNice = -ioNice;

      ioNice = (ioNice+20)%256;
      ioNice |= (((g_pCurrentModel->processesPriorities.ioNiceRouter+20) % 256)<<8);

      log_line("ionice video: %d, %d", ioNice, g_pCurrentModel->processesPriorities.ioNiceVideo);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
      {
         valuesToUI();
      }
      return;
   }

   if ( m_IndexIONiceValue == m_SelectedIndex )
   {
      int ioNice = ((m_pItemsSlider[3]->getCurrentValue()+20) % 256);
      ioNice |= (((g_pCurrentModel->processesPriorities.ioNiceRouter+20) % 256)<<8);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( (m_IndexEnableRadio == m_SelectedIndex) || (m_IndexRadioRx == m_SelectedIndex) || (m_IndexRadioTx == m_SelectedIndex) ||
        (m_IndexEnableRouter == m_SelectedIndex) || (m_IndexRouter == m_SelectedIndex) )
   {
      u32 uValues = 0;

      if ( m_pItemsSelect[11]->getSelectedIndex() != 0 )
         uValues |= m_pItemsSlider[11]->getCurrentValue() & 0xFF;
      if ( m_pItemsSelect[12]->getSelectedIndex() != 0 )
      {
         uValues |= (m_pItemsSlider[12]->getCurrentValue() & 0xFF) << 8;
         uValues |= (m_pItemsSlider[13]->getCurrentValue() & 0xFF) << 16;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_THREADS_PRIORITIES, uValues , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexDHCP == m_SelectedIndex )
   {
      int val = 1;
      if ( m_pItemsSelect[5]->getSelectedIndex() == 0 )
         val = 0;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ENABLE_DHCP, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         char szTextW[256];
         if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
            sprintf(szTextW, "Your %s is armed. Are you sure you want to reboot the controller?", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation", szTextW, 10);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
      return;
   }

   if ( g_pCurrentModel->processesPriorities.iFreqARM == 0 )
      g_pCurrentModel->processesPriorities.iFreqARM = 900;
   if ( g_pCurrentModel->processesPriorities.iFreqGPU == 0 )
      g_pCurrentModel->processesPriorities.iFreqGPU = 400;

   bool sendUpdate = false;
   command_packet_overclocking_params params;
   params.freq_arm = g_pCurrentModel->processesPriorities.iFreqARM;
   params.freq_gpu = g_pCurrentModel->processesPriorities.iFreqGPU;
   params.overvoltage = g_pCurrentModel->processesPriorities.iOverVoltage;
   params.uProcessesFlags = g_pCurrentModel->processesPriorities.uProcessesFlags;

   if ( m_IndexCPUEnabled == m_SelectedIndex )
   {
      params.freq_arm = -g_pCurrentModel->processesPriorities.iFreqARM;
      sendUpdate = true;
   }
   if ( m_IndexGPUEnabled == m_SelectedIndex )
   {
      params.freq_gpu = -g_pCurrentModel->processesPriorities.iFreqGPU;
      sendUpdate = true;
   }
   if ( m_IndexVoltageEnabled == m_SelectedIndex )
   {
      params.overvoltage = -g_pCurrentModel->processesPriorities.iOverVoltage;
      sendUpdate = true;
   }

   if ( m_IndexCPUSpeed == m_SelectedIndex )
   {
      params.freq_arm = m_pItemsSlider[7]->getCurrentValue();
      sendUpdate = true;
   }

   if ( m_IndexGPUSpeed == m_SelectedIndex )
   {
      params.freq_gpu = m_pItemsSlider[8]->getCurrentValue();
      sendUpdate = true;
   }

   if ( m_IndexVoltage == m_SelectedIndex )
   {
      params.overvoltage = m_pItemsSlider[9]->getCurrentValue();
      sendUpdate = true;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_CPU_SPEED, 0, NULL, 0) )
         valuesToUI();
   }
   
   if ( sendUpdate )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OVERCLOCKING_PARAMS, 0, (u8*)(&params), sizeof(command_packet_overclocking_params)) )
         valuesToUI();
   }

}
