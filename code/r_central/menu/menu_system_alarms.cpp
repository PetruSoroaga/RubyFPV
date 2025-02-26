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
#include "menu_system_alarms.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

static bool s_bMenuSystemAlarmsOnCustomOption = false;

MenuSystemAlarms::MenuSystemAlarms(void)
:Menu(MENU_ID_SYSTEM_ALARMS, "Configure System Alarms", NULL)
{
   m_Width = 0.37;
   m_yPos = 0.28;
   m_xPos = menu_get_XStartPos(m_Width);

   m_pItemsSelect[0] = new MenuItemSelect("All Alarms", "Turn all alarms on or off.");
   m_pItemsSelect[0]->addSelection("Disabled");
   m_pItemsSelect[0]->addSelection("Enabled");
   m_pItemsSelect[0]->addSelection("Custom");
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexAllAlarms = addMenuItem( m_pItemsSelect[0]);

   addSeparator();

   m_pItemsSelect[8] = new MenuItemSelect("Video Stream Alarms", "Turn alarms about video stream output on or off.");
   m_pItemsSelect[8]->addSelection("Disabled");
   m_pItemsSelect[8]->addSelection("Enabled");
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_IndexAlarmControllerIOErrors = addMenuItem( m_pItemsSelect[8]);


   m_pItemsSelect[1] = new MenuItemSelect("Invalid Radio Data", "Turn this alarm on or off.");
   m_pItemsSelect[1]->addSelection("Disabled");
   m_pItemsSelect[1]->addSelection("Enabled");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexAlarmInvalidRadioPackets = addMenuItem( m_pItemsSelect[1]);

   m_pItemsSelect[5] = new MenuItemSelect("Controller Link Lost", "Turn this alarm on or off.");
   m_pItemsSelect[5]->addSelection("Disabled");
   m_pItemsSelect[5]->addSelection("Enabled");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexAlarmControllerLink = addMenuItem( m_pItemsSelect[5]);
   
   m_pItemsSelect[2] = new MenuItemSelect("Video Overload", "Turn this alarm on or off.");
   m_pItemsSelect[2]->addSelection("Disabled");
   m_pItemsSelect[2]->addSelection("Enabled");
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexAlarmVideoDataOverload = addMenuItem( m_pItemsSelect[2]);

   m_pItemsSelect[3] = new MenuItemSelect("Vehicle CPU or Rx Overload", "Turn this alarm on or off.");
   m_pItemsSelect[3]->addSelection("Disabled");
   m_pItemsSelect[3]->addSelection("Enabled");
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexAlarmVehicleCPUOverload = addMenuItem( m_pItemsSelect[3]);

   m_pItemsSelect[6] = new MenuItemSelect("Vehicle Radio Timeout", "Alarm when a vehicle radio interface times out reading a pending received packet. Turn this alarm on or off.");
   m_pItemsSelect[6]->addSelection("Disabled");
   m_pItemsSelect[6]->addSelection("Enabled");
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexAlarmVehicleRxTimeout = addMenuItem( m_pItemsSelect[6]);

   addSeparator();

   m_pItemsSelect[4] = new MenuItemSelect("Controller CPU or Rx Overload", "Turn this alarm on or off.");
   m_pItemsSelect[4]->addSelection("Disabled");
   m_pItemsSelect[4]->addSelection("Enabled");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexAlarmControllerCPUOverload = addMenuItem( m_pItemsSelect[4]);

   m_pItemsSelect[7] = new MenuItemSelect("Vehicle Radio Timeout", "Alarm when a controller radio interface times out reading a pending received packet. Turn this alarm on or off.");
   m_pItemsSelect[7]->addSelection("Disabled");
   m_pItemsSelect[7]->addSelection("Enabled");
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_IndexAlarmControllerRxTimeout = addMenuItem( m_pItemsSelect[7]);

   m_pItemsSelect[9] = new MenuItemSelect("Developer Alarms", "Disables/Enables developer alarms.");
   m_pItemsSelect[9]->addSelection("Disabled");
   m_pItemsSelect[9]->addSelection("Enabled");
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_IndexAlarmsDev = addMenuItem( m_pItemsSelect[9]);
}

void MenuSystemAlarms::valuesToUI()
{
   
   Preferences* pP = get_Preferences();

   if ( s_bMenuSystemAlarmsOnCustomOption )
      m_pItemsSelect[0]->setSelectedIndex(2);
   else if ( 0 == pP->uEnabledAlarms )
      m_pItemsSelect[0]->setSelectedIndex(0);
   else if ( 0xFFFFFFFF == pP->uEnabledAlarms )
      m_pItemsSelect[0]->setSelectedIndex(1);
   else
      m_pItemsSelect[0]->setSelectedIndex(2);

   m_pItemsSelect[8]->setEnabled(true);
   m_pItemsSelect[8]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_CONTROLLER_IO_ERROR)?1:0);

   m_pItemsSelect[1]->setEnabled(true);
   m_pItemsSelect[1]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET)?1:0);

   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[2]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD)?1:0);

   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[3]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD)?1:0);

   m_pItemsSelect[4]->setEnabled(true);
   m_pItemsSelect[4]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD)?1:0);

   m_pItemsSelect[5]->setEnabled(true);
   m_pItemsSelect[5]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST)?1:0);

   m_pItemsSelect[6]->setEnabled(true);
   m_pItemsSelect[6]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_VEHICLE_RX_TIMEOUT)?1:0);

   m_pItemsSelect[7]->setEnabled(true);
   m_pItemsSelect[7]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_CONTROLLER_RX_TIMEOUT)?1:0);

   m_pItemsSelect[9]->setEnabled(true);
   m_pItemsSelect[9]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_DEVELOPER_ALARM)?1:0);

   if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
   {
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      m_pItemsSelect[6]->setEnabled(false);
      m_pItemsSelect[7]->setEnabled(false);
      m_pItemsSelect[8]->setEnabled(false);
      m_pItemsSelect[9]->setEnabled(false);

      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSelect[2]->setSelectedIndex(0);
      m_pItemsSelect[3]->setSelectedIndex(0);
      m_pItemsSelect[4]->setSelectedIndex(0);
      m_pItemsSelect[5]->setSelectedIndex(0);
      m_pItemsSelect[6]->setSelectedIndex(0);
      m_pItemsSelect[7]->setSelectedIndex(0);
      m_pItemsSelect[8]->setSelectedIndex(0);
      m_pItemsSelect[9]->setSelectedIndex(0);
   }   
}

void MenuSystemAlarms::onShow()
{
   Menu::onShow();
}


void MenuSystemAlarms::Render()
{
   RenderPrepare();
   float yEnd = RenderFrameAndTitle();
   float y = yEnd;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yEnd);
}

void MenuSystemAlarms::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;
   
   Preferences* pP = get_Preferences();

   if ( m_IndexAllAlarms == m_SelectedIndex )
   {
      s_bMenuSystemAlarmsOnCustomOption = false;
      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         pP->uEnabledAlarms = 0;
      else if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
         pP->uEnabledAlarms = 0xFFFFFFFF;
      else
        s_bMenuSystemAlarmsOnCustomOption = true;
   }

   if ( m_IndexAlarmControllerIOErrors == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~ALARM_ID_CONTROLLER_IO_ERROR;
      else
         pP->uEnabledAlarms |= ALARM_ID_CONTROLLER_IO_ERROR;
   }

   if ( m_IndexAlarmInvalidRadioPackets == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET | ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED | ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET);
      else
         pP->uEnabledAlarms |= (ALARM_ID_RECEIVED_INVALID_RADIO_PACKET | ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED | ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET);
   }

   if ( m_IndexAlarmControllerLink == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_LINK_TO_CONTROLLER_LOST | ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD);
      else
         pP->uEnabledAlarms |= (ALARM_ID_LINK_TO_CONTROLLER_LOST | ALARM_ID_LINK_TO_CONTROLLER_RECOVERED);
   }

   if ( m_IndexAlarmVideoDataOverload == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD | ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD);
      else
         pP->uEnabledAlarms |= (ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD | ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD);
   }

   if ( m_IndexAlarmVehicleCPUOverload == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         pP->uEnabledAlarms &= (~(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD)) && (~(ALARM_ID_CPU_RX_LOOP_OVERLOAD));
      else
         pP->uEnabledAlarms |= (ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD | ALARM_ID_CPU_RX_LOOP_OVERLOAD);
   }

   if ( m_IndexAlarmVehicleRxTimeout == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_VEHICLE_RX_TIMEOUT);
      else
         pP->uEnabledAlarms |= (ALARM_ID_VEHICLE_RX_TIMEOUT);
   }


   if ( m_IndexAlarmControllerCPUOverload == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         pP->uEnabledAlarms &= (~(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD)) && (~(ALARM_ID_CPU_RX_LOOP_OVERLOAD));
      else
         pP->uEnabledAlarms |= (ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD | ALARM_ID_CPU_RX_LOOP_OVERLOAD);
   }

   if ( m_IndexAlarmControllerRxTimeout == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_CONTROLLER_RX_TIMEOUT);
      else
         pP->uEnabledAlarms |= (ALARM_ID_CONTROLLER_RX_TIMEOUT);
   }


   save_Preferences();
   valuesToUI();
}

