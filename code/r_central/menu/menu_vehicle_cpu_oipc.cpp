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
#include "menu_vehicle_cpu_oipc.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"

MenuVehicleCPU_OIPC::MenuVehicleCPU_OIPC(void)
:Menu(MENU_ID_VEHICLE_CPU_OIPC, "CPU Settings", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;
   float fSliderWidth = 0.10;
   setSubTitle("Change vehicle processes priorities, for expert users.");

   m_IndexBalanceIntCores = -1;

   m_pItemsSlider[5] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency.", 700, 1200, 900, fSliderWidth);
   m_pItemsSlider[5]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[5]);
   
   m_pItemsSelect[5] = new MenuItemSelect("GPU Boost", "Increases the video encoder clock speed.");
   m_pItemsSelect[5]->addSelection("Off");
   m_pItemsSelect[5]->addSelection("Medium");
   m_pItemsSelect[5]->addSelection("High");
   m_pItemsSelect[5]->addSelection("Custom");
   m_pItemsSelect[5]->setIsEditable();
   m_IndexGPUBoost = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[7] = new MenuItemSelect("GPU Freq Core 1", "Set custom freq for GPU core 1");
   m_pItemsSelect[7]->addSelection("384 Mhz");
   m_pItemsSelect[7]->addSelection("432 Mhz");
   m_pItemsSelect[7]->addSelection("480 Mhz");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexGPUFreqCore1 = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[8] = new MenuItemSelect("GPU Freq Core 2", "Set custom freq for GPU core 2");
   m_pItemsSelect[8]->addSelection("320 Mhz");
   m_pItemsSelect[8]->addSelection("336 Mhz");
   m_pItemsSelect[8]->addSelection("348 Mhz");
   m_pItemsSelect[8]->addSelection("384 Mhz");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexGPUFreqCore2 = addMenuItem(m_pItemsSelect[8]);

   addMenuItem(new MenuItemSection("Priorities"));

   m_pItemsSelect[0] = new MenuItemSelect("Core Priority Adjustment", "Change the way the priority of Ruby processes is adjusted.");
   m_pItemsSelect[0]->addSelection("Default");
   m_pItemsSelect[0]->addSelection("Manual");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexEnableNice = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[0] = new MenuItemSlider("   Core Priority", "Sets the priority for the Ruby core functionality. Higher values means higher priority.", 1,18,11, fSliderWidth);
   m_IndexNiceRouter = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Core Threads Adjustment", "Change the way the priority of Ruby threads is adjusted.");
   m_pItemsSelect[1]->addSelection("Default");
   m_pItemsSelect[1]->addSelection("Manual");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexEnableRouter = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSlider[1] = new MenuItemSlider("   Threads Priority", "Sets the priority for the Ruby threads. Higher values means higher priority.", 1,90,10, fSliderWidth);
   m_IndexRouter = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Radio Threads Adjustment", "Change the way the priority of Ruby radio threads is adjusted.");
   m_pItemsSelect[2]->addSelection("Default");
   m_pItemsSelect[2]->addSelection("Custom");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexEnableRadio = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[2] = new MenuItemSlider("   Rx Threads Priority", "Sets the priority for the Ruby radio Rx threads. Higher values means higher priority.", 1,90,10, fSliderWidth);
   m_IndexRadioRx = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSlider[3] = new MenuItemSlider("   Tx Threads Priority", "Sets the priority for the Ruby radio Rx threads. Higher values means higher priority.", 1,90,10, fSliderWidth);
   m_IndexRadioTx = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSelect[4] = new MenuItemSelect("Video Priority Adjustment", "Change the way the priority of video processes is adjusted.");
   m_pItemsSelect[4]->addSelection("Default");
   m_pItemsSelect[4]->addSelection("Manual");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexEnableVideo = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSlider[4] = new MenuItemSlider("   Video Priority", "Sets the priority for the video processes. Higher values means higher priority.", 1,18,11, fSliderWidth);
   m_IndexNiceVideo = addMenuItem(m_pItemsSlider[4]);

   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      m_pItemsSelect[6] = new MenuItemSelect("Balance CPU Interrupts", "Tries to balance the load on the CPU cores interrupts.");
      m_pItemsSelect[6]->addSelection("Off");
      m_pItemsSelect[6]->addSelection("On");
      m_pItemsSelect[6]->setIsEditable();
      m_IndexBalanceIntCores = addMenuItem(m_pItemsSelect[6]);
   }
}

void MenuVehicleCPU_OIPC::valuesToUI()
{
   if ( g_pCurrentModel->processesPriorities.iFreqARM > 0 )
      m_pItemsSlider[5]->setCurrentValue(g_pCurrentModel->processesPriorities.iFreqARM);

   m_pItemsSelect[5]->setSelectedIndex(0);
   if ( g_pCurrentModel->processesPriorities.iFreqGPU < 3 )
   {
      m_pItemsSelect[5]->setSelectedIndex(g_pCurrentModel->processesPriorities.iFreqGPU);
      m_pItemsSelect[7]->setEnabled(false);
      m_pItemsSelect[8]->setEnabled(false);

      if ( 0 == g_pCurrentModel->processesPriorities.iFreqGPU )
      {
         m_pItemsSelect[7]->setSelectedIndex(0);
         m_pItemsSelect[8]->setSelectedIndex(0);
      }
      if ( 1 == g_pCurrentModel->processesPriorities.iFreqGPU )
      {
         m_pItemsSelect[7]->setSelectedIndex(1);
         m_pItemsSelect[8]->setSelectedIndex(1);
      }
      if ( 2 == g_pCurrentModel->processesPriorities.iFreqGPU )
      {
         m_pItemsSelect[7]->setSelectedIndex(2);
         m_pItemsSelect[8]->setSelectedIndex(2);
      }
   }
   else
   {
      m_pItemsSelect[5]->setSelectedIndex(3);
      m_pItemsSelect[7]->setEnabled(true);
      m_pItemsSelect[8]->setEnabled(true);

      int iCore1 = (g_pCurrentModel->processesPriorities.iFreqGPU/10)%10;
      int iCore2 = (g_pCurrentModel->processesPriorities.iFreqGPU/100)%10;
      m_pItemsSelect[7]->setSelectedIndex(iCore1);
      m_pItemsSelect[8]->setSelectedIndex(iCore2);
   }

   if ( g_pCurrentModel->processesPriorities.iNiceRouter == 0 )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSlider[0]->setCurrentValue(1);
      m_pItemsSlider[0]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[0]->setSelectedIndex(1);
      m_pItemsSlider[0]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceRouter);
      m_pItemsSlider[0]->setEnabled(true);
   }

   if ( g_pCurrentModel->processesPriorities.iThreadPriorityRouter == 0 )
   {
      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSlider[1]->setCurrentValue(1);
      m_pItemsSlider[1]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[1]->setSelectedIndex(1);
      m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
      m_pItemsSlider[1]->setEnabled(true);
   }

   if ( g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx == 0 )
   {
      m_pItemsSelect[2]->setSelectedIndex(0);
      m_pItemsSlider[2]->setCurrentValue(1);
      m_pItemsSlider[2]->setEnabled(false);
      m_pItemsSlider[3]->setCurrentValue(1);
      m_pItemsSlider[3]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[2]->setSelectedIndex(1);
      m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx);
      m_pItemsSlider[2]->setEnabled(true);
      m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx);
      m_pItemsSlider[3]->setEnabled(true);
   }

   if ( g_pCurrentModel->processesPriorities.iNiceVideo == 0 )
   {
      m_pItemsSelect[4]->setSelectedIndex(0);
      m_pItemsSlider[4]->setCurrentValue(1);
      m_pItemsSlider[4]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[4]->setSelectedIndex(1);
      m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->processesPriorities.iNiceVideo);
      m_pItemsSlider[4]->setEnabled(true);
   }

   if ( -1 != m_IndexBalanceIntCores )
   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_BALANCE_INT_CORES )
         m_pItemsSelect[6]->setSelectedIndex(1);
      else
         m_pItemsSelect[6]->setSelectedIndex(0);
   }
}

void MenuVehicleCPU_OIPC::onShow()
{
   Menu::onShow();
}


void MenuVehicleCPU_OIPC::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleCPU_OIPC::onReturnFromChild(int iChildMenuId, int returnValue)
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

void MenuVehicleCPU_OIPC::send_threads_values()
{
   u32 uValues = 0;

   if ( m_pItemsSelect[1]->getSelectedIndex() != 0 )
      uValues |= m_pItemsSlider[1]->getCurrentValue() & 0xFF;
   if ( m_pItemsSelect[2]->getSelectedIndex() != 0 )
   {
      uValues |= (m_pItemsSlider[2]->getCurrentValue() & 0xFF) << 8;
      uValues |= (m_pItemsSlider[3]->getCurrentValue() & 0xFF) << 16;
   }
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_THREADS_PRIORITIES, uValues , NULL, 0) )
      valuesToUI();
}

void MenuVehicleCPU_OIPC::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( ! menu_check_current_model_ok_for_edit() )
      return;

   if ( (m_IndexEnableNice == m_SelectedIndex) || (m_IndexNiceRouter == m_SelectedIndex)  )
   {
      int nr = 0;
      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         nr = 0;
      else
         nr = -m_pItemsSlider[0]->getCurrentValue();
      int nv = g_pCurrentModel->processesPriorities.iNiceVideo;
      int nc = g_pCurrentModel->processesPriorities.iNiceRC;
      int no = g_pCurrentModel->processesPriorities.iNiceOthers;
      u32 val = (nv+20) + (no+20)*256 + (nr+20)*256*256 + (nc+20)*256*256*256;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUES, val , NULL, 0) )
         valuesToUI();
      return;
   }


   if ( (m_IndexEnableRouter == m_SelectedIndex) || (m_IndexRouter == m_SelectedIndex) )
   {
      send_threads_values();
      return;
   }

   if ( (m_IndexEnableRadio == m_SelectedIndex) || (m_IndexRadioRx == m_SelectedIndex) || (m_IndexRadioTx == m_SelectedIndex) )
   {
      send_threads_values();
      return;
   }

   if ( (m_IndexEnableVideo == m_SelectedIndex) || (m_IndexNiceVideo == m_SelectedIndex)  )
   {
      int nv = 0;
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         nv = 0;
      else
         nv = -m_pItemsSlider[4]->getCurrentValue();
      int nr = g_pCurrentModel->processesPriorities.iNiceRouter;
      int nc = g_pCurrentModel->processesPriorities.iNiceRC;
      int no = g_pCurrentModel->processesPriorities.iNiceOthers;
      u32 val = (nv+20) + (no+20)*256 + (nr+20)*256*256 + (nc+20)*256*256*256;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUES, val , NULL, 0) )
         valuesToUI();
      return;
   }

   bool sendUpdate = false;
   command_packet_overclocking_params params;
   params.freq_arm = g_pCurrentModel->processesPriorities.iFreqARM;
   params.freq_gpu = g_pCurrentModel->processesPriorities.iFreqGPU;
   params.overvoltage = g_pCurrentModel->processesPriorities.iOverVoltage;
   params.uProcessesFlags = g_pCurrentModel->processesPriorities.uProcessesFlags;
   
   if ( m_IndexCPUSpeed == m_SelectedIndex )
   {
      params.freq_arm = m_pItemsSlider[5]->getCurrentValue();
      sendUpdate = true;
   }
   if ( m_IndexGPUBoost == m_SelectedIndex )
   {
      params.freq_gpu = m_pItemsSelect[5]->getSelectedIndex();
      sendUpdate = true;
   }

   if ( (m_IndexGPUFreqCore1 == m_SelectedIndex) || (m_IndexGPUFreqCore2 == m_SelectedIndex) )
   {
      params.freq_gpu = 3;
      params.freq_gpu += m_pItemsSelect[7]->getSelectedIndex()*10;
      params.freq_gpu += m_pItemsSelect[8]->getSelectedIndex()*100;
      sendUpdate = true;
   }

   if ( -1 != m_IndexBalanceIntCores )
   if ( m_IndexBalanceIntCores == m_SelectedIndex )
   {
      if ( 1 == m_pItemsSelect[6]->getSelectedIndex() )
         params.uProcessesFlags |= PROCESSES_FLAGS_BALANCE_INT_CORES;
      else
         params.uProcessesFlags &= ~PROCESSES_FLAGS_BALANCE_INT_CORES;

      sendUpdate = true;
   }

   if ( sendUpdate )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OVERCLOCKING_PARAMS, 0, (u8*)(&params), sizeof(command_packet_overclocking_params)) )
         valuesToUI();
   }
}
