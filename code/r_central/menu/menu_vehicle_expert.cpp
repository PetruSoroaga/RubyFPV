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
#include "menu_vehicle_expert.h"
#include "menu_txinfo.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"

MenuVehicleExpert::MenuVehicleExpert(void)
:Menu(MENU_ID_VEHICLE_EXPERT, "Expert Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;
   m_iConfirmationId = 0;
   float fSliderWidth = 0.10;
   char szBuff[32];
   setSubTitle("Change advanced vehicle settings, for expert users.");
   
   addTopInfo();

   addMenuItem(new MenuItemSection("Priorities"));

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

   addMenuItem(new MenuItemSection("Overclocking"));

   m_pItemsSelect[2] = new MenuItemSelect("Enable CPU Overclocking", "Enables overclocking of the main ARM CPU.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_IndexCPUEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[7] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency. Requires a reboot.", 700,1600, 1050, fSliderWidth);
   m_pItemsSlider[7]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[7]);

   m_pItemsSelect[3] = new MenuItemSelect("Enable GPU Overclocking", "Enables overclocking of the GPU cores.");  
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
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
   m_pItemsSlider[0]->setCurrentValue(-g_pCurrentModel->niceRouter);
   m_pItemsSlider[2]->setCurrentValue(-g_pCurrentModel->niceVideo);
   m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->niceTelemetry);
   m_pItemsSlider[5]->setCurrentValue(-g_pCurrentModel->niceRC);
   m_pItemsSlider[6]->setCurrentValue(-g_pCurrentModel->niceOthers);

   log_line("menu rc nice: %d", g_pCurrentModel->niceRC);
   m_pItemsSelect[0]->setSelection(g_pCurrentModel->ioNiceRouter > 0);
   if ( g_pCurrentModel->ioNiceRouter > 0 )
      m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->ioNiceRouter);
   else
      m_pItemsSlider[1]->setCurrentValue(-g_pCurrentModel->ioNiceRouter);
   m_pItemsSlider[1]->setEnabled(g_pCurrentModel->ioNiceRouter > 0);

   m_pItemsSelect[1]->setSelection(g_pCurrentModel->ioNiceVideo > 0);
   if ( g_pCurrentModel->ioNiceVideo > 0 )
      m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->ioNiceVideo);
   else
      m_pItemsSlider[3]->setCurrentValue(-g_pCurrentModel->ioNiceVideo);
   m_pItemsSlider[3]->setEnabled(g_pCurrentModel->ioNiceVideo > 0);

   // CPU

   m_pItemsSelect[2]->setSelection(g_pCurrentModel->iFreqARM > 0);
   if ( g_pCurrentModel->iFreqARM > 0 )
      m_pItemsSlider[7]->setCurrentValue(g_pCurrentModel->iFreqARM);
   else
      m_pItemsSlider[7]->setCurrentValue(-g_pCurrentModel->iFreqARM);
   m_pItemsSlider[7]->setEnabled(g_pCurrentModel->iFreqARM > 0);

   m_pItemsSelect[3]->setSelection(g_pCurrentModel->iFreqGPU > 0);
   if ( g_pCurrentModel->iFreqGPU > 0 )
      m_pItemsSlider[8]->setCurrentValue(g_pCurrentModel->iFreqGPU);
   else
      m_pItemsSlider[8]->setCurrentValue(-g_pCurrentModel->iFreqGPU);
   m_pItemsSlider[8]->setEnabled(g_pCurrentModel->iFreqGPU > 0);

   m_pItemsSelect[4]->setSelection(g_pCurrentModel->iOverVoltage > 0);
   if ( g_pCurrentModel->iOverVoltage > 0 )
      m_pItemsSlider[9]->setCurrentValue(g_pCurrentModel->iOverVoltage);
   else
      m_pItemsSlider[9]->setCurrentValue(-g_pCurrentModel->iOverVoltage);
   m_pItemsSlider[9]->setEnabled(g_pCurrentModel->iOverVoltage > 0);   


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


void MenuVehicleExpert::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 10 == m_iConfirmationId && 1 == returnValue )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_close_all();
      m_iConfirmationId = 0;
      return;
   }

   m_iConfirmationId = 0;
}

void MenuVehicleExpert::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexNiceRouter == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
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


   if ( m_IndexNiceVideo == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
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

   if ( m_IndexNiceRC == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
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

   if ( m_IndexNiceOthers == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
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

   if ( m_IndexNiceTelemetry == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int nt = -m_pItemsSlider[4]->getCurrentValue();
      u32 val = nt+20;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_NICE_VALUE_TELEMETRY, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexIONiceRouter == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int ioNiceRouter = g_pCurrentModel->ioNiceRouter;
      if ( ioNiceRouter < 0 )
         ioNiceRouter = -ioNiceRouter;
      if ( ioNiceRouter == 0 )
         ioNiceRouter = 5;

      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         ioNiceRouter = -ioNiceRouter;
      log_line("ionice router: %d, %d", ioNiceRouter, g_pCurrentModel->ioNiceRouter);

      int ioNice = ((g_pCurrentModel->ioNiceVideo+20) % 256);
      ioNice |= (((ioNiceRouter+20) % 256)<<8);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
      {
         valuesToUI();
      }
      return;
   }

   if ( m_IndexIONiceRouterValue == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int ioNice = ((g_pCurrentModel->ioNiceVideo+20) % 256);
      ioNice |= (((m_pItemsSlider[1]->getCurrentValue()+20) % 256)<<8);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
         valuesToUI();
      return;
   }



   if ( m_IndexIONice == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int ioNice = g_pCurrentModel->ioNiceVideo;
      if ( ioNice < 0 )
         ioNice = -ioNice;
      if ( ioNice == 0 )
         ioNice = 5;

      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         ioNice = -ioNice;

      ioNice = (ioNice+20)%256;
      ioNice |= (((g_pCurrentModel->ioNiceRouter+20) % 256)<<8);

      log_line("ionice video: %d, %d", ioNice, g_pCurrentModel->ioNiceVideo);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
      {
         valuesToUI();
      }
      return;
   }

   if ( m_IndexIONiceValue == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int ioNice = ((m_pItemsSlider[3]->getCurrentValue()+20) % 256);
      ioNice |= (((g_pCurrentModel->ioNiceRouter+20) % 256)<<8);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_IONICE_VALUES, ioNice, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexDHCP == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
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
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_close_all();
      return;
   }

   if ( g_pCurrentModel->iFreqARM == 0 )
      g_pCurrentModel->iFreqARM = 900;
   if ( g_pCurrentModel->iFreqGPU == 0 )
      g_pCurrentModel->iFreqGPU = 400;

   bool sendUpdate = false;
   command_packet_overclocking_params params;
   params.freq_arm = g_pCurrentModel->iFreqARM;
   params.freq_gpu = g_pCurrentModel->iFreqGPU;
   params.overvoltage = g_pCurrentModel->iOverVoltage;

   if ( m_IndexCPUEnabled == m_SelectedIndex )
   {
      params.freq_arm = -g_pCurrentModel->iFreqARM;
      sendUpdate = true;
   }
   if ( m_IndexGPUEnabled == m_SelectedIndex )
   {
      params.freq_gpu = -g_pCurrentModel->iFreqGPU;
      sendUpdate = true;
   }
   if ( m_IndexVoltageEnabled == m_SelectedIndex )
   {
      params.overvoltage = -g_pCurrentModel->iOverVoltage;
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
