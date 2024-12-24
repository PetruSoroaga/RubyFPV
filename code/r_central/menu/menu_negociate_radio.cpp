/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

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
#include "menu_negociate_radio.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "../process_router_messages.h"

MenuNegociateRadio::MenuNegociateRadio(void)
:Menu(MENU_ID_NEGOCIATE_RADIO, "Initial Auto Radio Link Adjustment", NULL)
{
   m_Width = 0.6;
   m_xPos = 0.18; m_yPos = 0.26;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   addExtraHeightAtEnd(2.0*height_text + height_text * 1.5 * hardware_get_radio_interfaces_count());
   m_uShowTime = g_TimeNow;
   m_MenuIndexCancel = -1;
   m_iCounter = 0;
   addTopLine("Doing the initial radio link parameters adjustment for best performance...");
   addTopLine("(This is done on first installation and on first pairing with a vehicle or when hardware has changed on the vehicle)");

   strcpy(m_szStatusMessage, "Please wait, it will take a minute");
   addTopLine(m_szStatusMessage);
   free(m_szTopLines[2]);
   m_szTopLines[2] = (char*)malloc(256);
   strcpy(m_szTopLines[2], m_szStatusMessage);

   for( int i=0; i<20; i++ )
   for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
   for( int j=0; j<3; j++)
   {
      m_iRXPackets[i][k][j] = -1;
      m_iRxLostPackets[i][k][j] = -1;
   }

   m_iDataRateToApply = 0;
   m_iDataRateIndex = 0;
   m_iDataRateTestCount = 0;
   m_iCountSucceededSteps = 0;
   m_iCountFailedSteps = 0;
   m_bWaitingCancelConfirmationFromUser = false;
   g_bAskedForNegociateRadioLink = true;
   _switch_to_step(NEGOCIATE_RADIO_STEP_DATA_RATE);
}

MenuNegociateRadio::~MenuNegociateRadio()
{
}

void MenuNegociateRadio::valuesToUI()
{
}

void MenuNegociateRadio::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float wPixel = g_pRenderEngine->getPixelWidth();
   float hPixel = g_pRenderEngine->getPixelHeight();
   y += height_text*0.5;
   g_pRenderEngine->setColors(get_Color_MenuText());
   float fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, "Computing Link Quality (you can press [Cancel] to cancel)");
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenu, "Computing Link Quality (you can press [Cancel] to cancel)");
   y += height_text*1.5;

   float hBar = height_text*1.5;
   float wBar = height_text*0.5;
   float x = m_RenderXPos+m_sfMenuPaddingX;

   x += 0.5*(m_RenderWidth - 2.0*m_sfMenuPaddingX - (wBar+4.0*wPixel)*3*g_ArrayTestRadioRatesCount);

   _computeQualities();

   int iIndexBestQuality = -1;
   int iRunBestQuality = -1;
   float fBestQ = 0;

   for( int iTest=0; iTest<g_ArrayTestRadioRatesCount; iTest++ )
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
   for( int iRun=0; iRun<3; iRun++ )
   {
      if ( !hardware_radio_is_index_wifi_radio(iInt) )
         continue;

      float fQuality = 0.0;
      if ( m_iRXPackets[iTest][iInt][iRun] + m_iRxLostPackets[iTest][iInt][iRun] > 0 )
         fQuality = (float)m_iRXPackets[iTest][iInt][iRun]/(float)(m_iRXPackets[iTest][iInt][iRun] + m_iRxLostPackets[iTest][iInt][iRun]);
      if ( m_iRXPackets[iTest][iInt][iRun] < 100 )
      if ( (iTest < m_iDataRateIndex) || ((iTest == m_iDataRateIndex) && (iRun <= m_iDataRateTestCount)) )
         fQuality = 0.1;

      if ( fQuality > fBestQ )
      {
         fBestQ = fQuality;
         iIndexBestQuality = iTest;
         iRunBestQuality = iRun;
      }
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + (iTest*3+iRun)*(wBar+4.0*wPixel), y + iInt*(hBar+hPixel*2.0), wBar, hBar-2.0*hPixel);

      if ( fQuality > 0.99 )
         g_pRenderEngine->setFill(0,200,0,1.0);
      else if ( fQuality > 0.95 )
         g_pRenderEngine->setColors(get_Color_MenuText());
      else if ( fQuality > 0.9 )
         g_pRenderEngine->setFill(200,200,0,1.0);
      else if ( fQuality > 0.0001 )
      {
         fQuality = 0.3;
         g_pRenderEngine->setFill(200,0,0,1.0);
      }
      else
         g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + (iTest*3+iRun)*(wBar+4.0*wPixel) + wPixel, y + iInt*(hBar+hPixel*2.0) + hBar - fQuality*hBar - hPixel, wBar - 2.0*wPixel, fQuality*(hBar-2.0*hPixel));
   }

   if ( iIndexBestQuality >= 0 )
   {
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + (iIndexBestQuality*3+iRunBestQuality)*(wBar+4.0*wPixel) - 3.0*wPixel,
                y - hPixel*4.0, wBar+5.0*wPixel, (hBar+2.0*hPixel)*hardware_get_radio_interfaces_count() + 6.0*hPixel);
   }

   RenderEnd(yTop);
}

int MenuNegociateRadio::onBack()
{
   if ( -1 != m_MenuIndexCancel )
   {
      _onCancel();
   }
   return 0;
}

void MenuNegociateRadio::_computeQualities()
{
   for( int iTest=0; iTest<g_ArrayTestRadioRatesCount; iTest++ )
   {
      m_fQualities[iTest] = 0.0;
      for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
      for( int iRun=0; iRun<3; iRun++ )
      {
         if ( !hardware_radio_is_index_wifi_radio(iInt) )
            continue;

         float fQuality = 0.0;
         if ( m_iRXPackets[iTest][iInt][iRun] + m_iRxLostPackets[iTest][iInt][iRun] > 0 )
            fQuality = (float)m_iRXPackets[iTest][iInt][iRun]/(float)(m_iRXPackets[iTest][iInt][iRun] + m_iRxLostPackets[iTest][iInt][iRun]);
         if ( m_iRXPackets[iTest][iInt][iRun] < 100 )
            fQuality = 0.1;

         if ( fQuality > m_fQualities[iTest] )
           m_fQualities[iTest] = fQuality;
      }
   }
}

void MenuNegociateRadio::_send_command_to_vehicle()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32);

   u8 buffer[1024];

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8 uType = 0;
   u32 uParam = 0;
   int iParam = 0;

   if ( m_iStep == NEGOCIATE_RADIO_STEP_DATA_RATE )
   {
      iParam = g_ArrayTestRadioRates[m_iDataRateIndex];
      memcpy(&uParam, &iParam, sizeof(int));
   }

   if ( m_iStep == NEGOCIATE_RADIO_STEP_END )
   {
      iParam = m_iDataRateToApply;
      memcpy(&uParam, &iParam, sizeof(int));
      log_line("Sending to vehicle final video radio datarate: %d (int size: %d, u32 size: %d)", m_iDataRateToApply, (int)sizeof(int), (int)sizeof(u32));
   }

   buffer[sizeof(t_packet_header)] = uType;
   buffer[sizeof(t_packet_header)+sizeof(u8)] = (u8)m_iStep;
   memcpy(&(buffer[0]) + sizeof(t_packet_header) + 2*sizeof(u8), &uParam, sizeof(u32));

   radio_packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);

   m_uLastTimeSendCommandToVehicle = g_TimeNow;
}

void MenuNegociateRadio::_switch_to_step(int iStep)
{
   m_iStep = iStep;

   if ( m_iStep == NEGOCIATE_RADIO_STEP_END )
   {
      strcpy(m_szStatusMessage, "Done. Saving parameters.");
      _apply_new_settings();
   }
   if ( m_iStep == NEGOCIATE_RADIO_STEP_CANCEL )
      strcpy(m_szStatusMessage, "Canceling, please wait.");

   m_bWaitingVehicleConfirmation = true;
   m_uStepStartTime = g_TimeNow;
   _send_command_to_vehicle();

}

void MenuNegociateRadio::_advance_to_next_step()
{
   if ( m_iStep != NEGOCIATE_RADIO_STEP_DATA_RATE )
      return;

   if ( m_iCountFailedSteps >= 6 )
   {
      log_softerror_and_alarm("Failed 6 data steps. Aborting operation.");
      _switch_to_step(NEGOCIATE_RADIO_STEP_CANCEL);
      return;
   }

   if ( m_iDataRateIndex >= g_ArrayTestRadioRatesCount )
   {
      _switch_to_step(NEGOCIATE_RADIO_STEP_END);
      return;    
   }    

   m_iDataRateTestCount++;
   if ( m_iDataRateTestCount > 2 )
   {
      m_iDataRateIndex++;
      m_iDataRateTestCount = 0;
   }
   if ( m_iDataRateIndex >= g_ArrayTestRadioRatesCount )
      m_iStep = NEGOCIATE_RADIO_STEP_END;
   _switch_to_step(m_iStep);
}

void MenuNegociateRadio::_apply_new_settings()
{
   m_iDataRateToApply = 0;

   _computeQualities();
   float fQuality18 = m_fQualities[0];
   float fQuality24 = m_fQualities[1];
   float fQuality48 = m_fQualities[3];
   float fQualityMCS2 = m_fQualities[6];
   float fQualityMCS3 = m_fQualities[7];
   float fQualityMCS4 = m_fQualities[8];
   
   if ( fQualityMCS4 > 0.99 )
      m_iDataRateToApply = -5;
   else if ( fQualityMCS3 > 0.99 )
      m_iDataRateToApply = -4;
   else if ( fQualityMCS2 > fQuality18 )
      m_iDataRateToApply = -3;
   else if ( fQuality48 > 0.99 )
      m_iDataRateToApply = 48000000;
   else if ( fQuality24 > 0.99 )
      m_iDataRateToApply = 24000000;
   else
      m_iDataRateToApply = 18000000;

   log_line("Computed Q: 18: %.3f, 24: %.3f, 48: %.3f, MCS2: %.3f, MCS3: %.3f, MCS4: %.3f ",
      fQuality18, fQuality24, fQuality48, fQualityMCS2, fQualityMCS3, fQualityMCS4);
   log_line("Appling datarate: %d (%u bps)", m_iDataRateToApply, getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0));
   log_line("Max/Min video bitrate interval to set: %u / %u bps",
     (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 ) * DEFAULT_VIDEO_LINK_LOAD_PERCENT,
     (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 ) * 30 );

   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         if ( (g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps / 100) * DEFAULT_VIDEO_LINK_LOAD_PERCENT > getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) )
            g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps = (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 )* DEFAULT_VIDEO_LINK_LOAD_PERCENT;
      
         if ( (i == VIDEO_PROFILE_USER) || (i == VIDEO_PROFILE_HIGH_QUALITY) )
         if ( (g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps / 100) * 30 < getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) )
            g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps = (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 ) * 30;
      }
   }
}

bool MenuNegociateRadio::periodicLoop()
{
   m_iCounter++;
   if ( -1 == m_MenuIndexCancel )
   if ( g_TimeNow > m_uShowTime + 4000 )
   {
      m_MenuIndexCancel = addMenuItem(new MenuItem("Cancel", "Aborts the autoadjustment procedure without making any changes."));
      invalidate();
   }

   strcpy(m_szTopLines[2], m_szStatusMessage);
   for(int i=0; i<(m_iCounter%4); i++ )
      strcat(m_szTopLines[2], ".");

   if ( m_bWaitingVehicleConfirmation )
   {
      if ( (g_TimeNow > m_uLastTimeSendCommandToVehicle + 100) && (!m_bWaitingCancelConfirmationFromUser) && (g_TimeNow < m_uStepStartTime + 3000) )
         _send_command_to_vehicle();       
      if ( (g_TimeNow > m_uStepStartTime + 3000) && (!m_bWaitingCancelConfirmationFromUser) )
      {
         m_iCountFailedSteps++;
         if ( (m_iStep == NEGOCIATE_RADIO_STEP_CANCEL) || (m_iStep == NEGOCIATE_RADIO_STEP_END) )
         {
            if ( m_iCountFailedSteps >= 6 )
            {
               setModal(false);
               menu_stack_pop(0);
               m_bWaitingCancelConfirmationFromUser = true;
               log_line("Timing out operation with 6 failed steps.");
               addMessage2(0, "Failed to negociate radio links.", "You radio links quality is very poor. Please fix the physical radio links quality and try again later.");
            }
            else
            {
               setModal(false);
               menu_stack_pop(0);
            }
            return false;
         }
         else
            _advance_to_next_step();
      }
   }
   else
   {
      if ( m_iStep == NEGOCIATE_RADIO_STEP_DATA_RATE )
      {
         for(int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( !hardware_radio_is_index_wifi_radio(i) )
               continue;
            m_iRXPackets[m_iDataRateIndex][i][m_iDataRateTestCount] = g_SM_RadioStats.radio_interfaces[i].totalRxPackets;
            m_iRxLostPackets[m_iDataRateIndex][i][m_iDataRateTestCount] = g_SM_RadioStats.radio_interfaces[i].totalRxPacketsBad + g_SM_RadioStats.radio_interfaces[i].totalRxPacketsLost;
         }
         if ( g_TimeNow > m_uStepStartTime + 2000 )
         {
            m_iCountSucceededSteps++;
            _advance_to_next_step();
         }
      }
      return false;
   }

   if ( m_iStep == NEGOCIATE_RADIO_STEP_CANCEL )
   if ( g_TimeNow > m_uStepStartTime + 5000 )
   {
      setModal(false);
      menu_stack_pop(0);
   }
   return false;
}

void MenuNegociateRadio::onReceivedVehicleResponse(u8* pPacketData, int iPacketLength)
{
   if ( NULL == pPacketData )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketData;

   if ( pPH->packet_type != PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
      return;

   u8 uCommand = pPacketData[sizeof(t_packet_header) + sizeof(u8)];
   u32 uParam = 0;
   int iParam = 0;
   memcpy(&uParam, pPacketData + sizeof(t_packet_header) + 2*sizeof(u8), sizeof(u32));
   memcpy(&iParam, &uParam, sizeof(int));
   log_line("Received negociate radio link conf, command %d, param: %d", uCommand, iParam);
      

   if ( uCommand != m_iStep )
      return;
   if ( (uCommand == NEGOCIATE_RADIO_STEP_END) || (uCommand == NEGOCIATE_RADIO_STEP_CANCEL  ) )
   {
      if ( uCommand == NEGOCIATE_RADIO_STEP_END )
      {
         log_line("Saving new video radio datarate on current model: %d", m_iDataRateToApply);
         g_pCurrentModel->resetVideoLinkProfilesToDataRates(m_iDataRateToApply, m_iDataRateToApply);
         g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         g_bMustNegociateRadioLinksFlag = false;
      }
      if ( (uCommand == NEGOCIATE_RADIO_STEP_CANCEL) && (m_iCountFailedSteps >= 6) )
      {
         setModal(false);
         menu_stack_pop(0);
         m_bWaitingCancelConfirmationFromUser = true;
         log_line("Finishing up operation with 6 failed steps.");
         addMessage2(0, "Failed to negociate radio links.", "You radio links quality is very poor. Please fix the physical radio links quality and try again later.");
      }
      else
      {
         setModal(false);
         menu_stack_pop(0);
      }
   }

   m_bWaitingVehicleConfirmation = false;
}

void MenuNegociateRadio::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   if ( m_bWaitingCancelConfirmationFromUser )
   {
      log_line("User confirmed canceled operation. Close menu.");
      //setModal(false);
      menu_stack_pop(0);
      menu_rearrange_all_menus_no_animation();
   }
}

void MenuNegociateRadio::_onCancel()
{
   if ( -1 != m_MenuIndexCancel )
   {
      removeMenuItem(m_pMenuItems[0]);
      m_MenuIndexCancel = -1;
   }
   _switch_to_step(NEGOCIATE_RADIO_STEP_CANCEL);
}

void MenuNegociateRadio::onSelectItem()
{
   Menu::onSelectItem();
   if ( -1 == m_SelectedIndex )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( (-1 != m_MenuIndexCancel) && (m_MenuIndexCancel == m_SelectedIndex) )
   {
      _onCancel();
   }
}
