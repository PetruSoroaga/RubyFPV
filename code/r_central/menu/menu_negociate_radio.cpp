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
#include "menu_negociate_radio.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "../process_router_messages.h"

int s_ArrayTestRadioRates[] = {6000000, 12000000, 18000000, 24000000, 36000000, 48000000, -1, -2, -3, -4, -5};


MenuNegociateRadio::MenuNegociateRadio(void)
:Menu(MENU_ID_NEGOCIATE_RADIO, "Initial Auto Radio Link Adjustment", NULL)
{
   m_Width = 0.72;
   m_xPos = 0.14; m_yPos = 0.26;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   addExtraHeightAtEnd(3.5*height_text + height_text * 1.5 * hardware_get_radio_interfaces_count());
   m_uShowTime = g_TimeNow;
   m_MenuIndexCancel = -1;
   m_iLoopCounter = 0;
   addTopLine("Doing the initial radio link parameters adjustment for best performance...");
   addTopLine("(This is done on first installation and on first pairing with a vehicle or when hardware has changed on the vehicle)");

   strcpy(m_szStatusMessage, L("Please wait, it will take a minute (you can press [Cancel] to cancel)."));
   strcpy(m_szStatusMessage2, L("Computing Link Quality"));

   for( int i=0; i<MAX_NEGOCIATE_TESTS; i++ )
   {
      m_TestStepsInfo[i].iDataRateToTest = 0;
      m_TestStepsInfo[i].uFlagsToTest = 0;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         m_TestStepsInfo[i].iRadioInterfacesRXPackets[k] = -1;
         m_TestStepsInfo[i].iRadioInterfacesRxLostPackets[k] = -1;
         m_TestStepsInfo[i].fQualityCards[k] = 0;
      }
      m_TestStepsInfo[i].fComputedQuality = 0;
   }

   m_iTestsCount = 0;
   for( int i=0; i<(int)(sizeof(s_ArrayTestRadioRates)/sizeof(s_ArrayTestRadioRates[0])); i++ )
   {
      for( int k=0; k<3; k++ )
      {
         m_TestStepsInfo[m_iTestsCount].iDataRateToTest = s_ArrayTestRadioRates[i];
         m_iTestsCount++;
      }
   }
   m_iIndexFirstRadioFlagsTest = m_iTestsCount;
   for( int k=0; k<3; k++ )
   {
      m_TestStepsInfo[m_iTestsCount].iDataRateToTest = -3;
      m_TestStepsInfo[m_iTestsCount].uFlagsToTest = RADIO_FLAG_STBC_VEHICLE | RADIO_FLAGS_USE_MCS_DATARATES;
      m_TestStepsInfo[m_iTestsCount].uFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
      m_iTestsCount++;
   }
   for( int k=0; k<3; k++ )
   {
      m_TestStepsInfo[m_iTestsCount].iDataRateToTest = -3;
      m_TestStepsInfo[m_iTestsCount].uFlagsToTest = RADIO_FLAG_LDPC_VEHICLE | RADIO_FLAGS_USE_MCS_DATARATES;
      m_TestStepsInfo[m_iTestsCount].uFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
      m_iTestsCount++;
   }
   for( int k=0; k<3; k++ )
   {
      m_TestStepsInfo[m_iTestsCount].iDataRateToTest = -3;
      m_TestStepsInfo[m_iTestsCount].uFlagsToTest = RADIO_FLAG_STBC_VEHICLE | RADIO_FLAG_LDPC_VEHICLE | RADIO_FLAGS_USE_MCS_DATARATES;
      m_TestStepsInfo[m_iTestsCount].uFlagsToTest &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES);
      m_iTestsCount++;
   }

   m_iStep = NEGOCIATE_RADIO_STEP_DATA_RATE;
   m_iCurrentTestIndex = 0;
   m_fQualityOfDataRateToApply = 0.0;
   m_iIndexTestDataRateToApply = -1;
   m_iDataRateToApply = 0;
   m_uRadioFlagsToApply = g_pCurrentModel->radioLinksParams.link_radio_flags[0];
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
   
   float fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_szStatusMessage);
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenu, m_szStatusMessage);

   y += height_text*1.5;
   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_szStatusMessage2);
   g_pRenderEngine->drawText(m_RenderXPos+m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenu, m_szStatusMessage2);
   char szBuff[32];
   szBuff[0] = 0;
   for(int i=0; i<(m_iLoopCounter%4); i++ )
      strcat(szBuff, ".");
   g_pRenderEngine->drawText(m_RenderXPos + fTextWidth + m_sfMenuPaddingX + 0.5 * (m_RenderWidth-2.0*m_sfMenuPaddingX - fTextWidth), y, g_idFontMenu, szBuff);

   y += height_text*1.5;

   float hBar = height_text*1.5;
   float wBar = height_text*0.5;
   float x = m_RenderXPos+m_sfMenuPaddingX;

   x += 0.5*(m_RenderWidth - 2.0*m_sfMenuPaddingX - (wBar+4.0*wPixel)*m_iTestsCount);

   _computeQualities();

   int iIndexBestQuality = -1;
   float fBestQ = 0;

   for( int iTest=0; iTest<m_iTestsCount; iTest++ )
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
   {
      if ( !hardware_radio_index_is_wifi_radio(iInt) )
         continue;

      float fQualityCard = m_TestStepsInfo[iTest].fQualityCards[iInt];

      if ( fQualityCard > fBestQ )
      if ( iTest < m_iCurrentTestIndex )
      {
         fBestQ = fQualityCard;
         iIndexBestQuality = iTest;
      }

      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + iTest*(wBar+4.0*wPixel), y + iInt*(hBar+hPixel*2.0), wBar, hBar-2.0*hPixel);

      float fRectH = 1.0;
      if ( fQualityCard > 0.99 )
         g_pRenderEngine->setFill(0,200,0,1.0);
      else if ( fQualityCard > 0.95 )
         g_pRenderEngine->setColors(get_Color_MenuText());
      else if ( fQualityCard > 0.9 )
         g_pRenderEngine->setFill(200,200,0,1.0);
      else if ( fQualityCard > 0.11 )
         g_pRenderEngine->setFill(200,100,100,1.0);
      else if ( fQualityCard > 0.0001 )
      {
         fRectH = 0.3;
         g_pRenderEngine->setFill(200,0,0,1.0);
      }
      else
      {
         fRectH = 0.0;
         g_pRenderEngine->setFill(0,0,0,0);
      }
      if ( fRectH > 0.001 )
         g_pRenderEngine->drawRect(x + iTest*(wBar+4.0*wPixel) + wPixel, y + iInt*(hBar+hPixel*2.0) + hBar - fRectH*hBar - hPixel, wBar - 2.0*wPixel, fRectH*(hBar-2.0*hPixel));
   }

   if ( m_iIndexTestDataRateToApply != -1 )
      iIndexBestQuality = m_iIndexTestDataRateToApply;

   if ( iIndexBestQuality >= 0 )
   {
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRect(x + iIndexBestQuality*(wBar+4.0*wPixel) - 3.0*wPixel,
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
   for( int iTest=0; iTest<=m_iCurrentTestIndex; iTest++ )
   {
      m_TestStepsInfo[iTest].fComputedQuality = 0.1;
      for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++ )
      {
         if ( !hardware_radio_index_is_wifi_radio(iInt) )
            continue;
         m_TestStepsInfo[iTest].fQualityCards[iInt] = 0.1;
         if ( m_TestStepsInfo[iTest].iRadioInterfacesRXPackets[iInt] + m_TestStepsInfo[iTest].iRadioInterfacesRxLostPackets[iInt] > 50 )
         {
            m_TestStepsInfo[iTest].fQualityCards[iInt]= (float)m_TestStepsInfo[iTest].iRadioInterfacesRXPackets[iInt]/((float)m_TestStepsInfo[iTest].iRadioInterfacesRXPackets[iInt] + (float)m_TestStepsInfo[iTest].iRadioInterfacesRxLostPackets[iInt]);
            if ( m_TestStepsInfo[iTest].fQualityCards[iInt] > m_TestStepsInfo[iTest].fComputedQuality )
              m_TestStepsInfo[iTest].fComputedQuality = m_TestStepsInfo[iTest].fQualityCards[iInt];
         }
      }
   }
}

float MenuNegociateRadio::_getComputedQualityForDatarate(int iDatarate, int* pTestIndex)
{
   float fQuality = 0.0;
   for( int iTest=0; iTest<m_iIndexFirstRadioFlagsTest; iTest++ )
   {
      if ( iTest > m_iCurrentTestIndex )
         break;
      if ( m_TestStepsInfo[iTest].iDataRateToTest != iDatarate )
         continue;

      if ( m_TestStepsInfo[iTest].fComputedQuality > fQuality )
      {
         fQuality = m_TestStepsInfo[iTest].fComputedQuality;
         if ( NULL != pTestIndex )
            *pTestIndex = iTest;
      }
   }
   return fQuality;
}


float MenuNegociateRadio::_getMinComputedQualityForDatarate(int iDatarate, int* pTestIndex)
{
   float fQuality = 100.0;
   for( int iTest=0; iTest<m_iIndexFirstRadioFlagsTest; iTest++ )
   {
      if ( iTest > m_iCurrentTestIndex )
         break;
      if ( m_TestStepsInfo[iTest].iDataRateToTest != iDatarate )
         continue;

      if ( m_TestStepsInfo[iTest].fComputedQuality < fQuality )
      {
         fQuality = m_TestStepsInfo[iTest].fComputedQuality;
         if ( NULL != pTestIndex )
            *pTestIndex = iTest;
      }
   }
   return fQuality;
}

void MenuNegociateRadio::_send_command_to_vehicle()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8) + 2*sizeof(u32);

   u8 buffer[1024];

   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   u8 uType = 0;
   u32 uParam1 = 0;
   u32 uParam2 = 0;
   int iParam1 = 0;

   if ( m_iStep == NEGOCIATE_RADIO_STEP_DATA_RATE )
   {
      iParam1 = m_TestStepsInfo[m_iCurrentTestIndex].iDataRateToTest;
      memcpy(&uParam1, &iParam1, sizeof(int));
      uParam2 = m_TestStepsInfo[m_iCurrentTestIndex].uFlagsToTest;
   }

   if ( m_iStep == NEGOCIATE_RADIO_STEP_END )
   {
      iParam1 = m_iDataRateToApply;
      memcpy(&uParam1, &iParam1, sizeof(int));
      uParam2 = m_uRadioFlagsToApply;
      log_line("Sending to vehicle final video radio datarate: %d (int size: %d, u32 size: %d), radio flags: %u (%s)", m_iDataRateToApply, (int)sizeof(int), (int)sizeof(u32), uParam2, str_get_radio_frame_flags_description2(uParam2));
   }

   buffer[sizeof(t_packet_header)] = uType;
   buffer[sizeof(t_packet_header)+sizeof(u8)] = (u8)m_iStep;
   memcpy(&(buffer[0]) + sizeof(t_packet_header) + 2*sizeof(u8), &uParam1, sizeof(u32));
   memcpy(&(buffer[0]) + sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32), &uParam2, sizeof(u32));

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

   if ( m_iCurrentTestIndex >= (m_iTestsCount-1) )
   {
      _switch_to_step(NEGOCIATE_RADIO_STEP_END);
      return;    
   }    

   m_iCurrentTestIndex++;
   _compute_datarate_settings_to_apply();
   //if ( m_TestStepsInfo[m_iCurrentTestIndex].uFlagsToTest != 0 )
   //if ( m_TestStepsInfo[m_iCurrentTestIndex-1].uFlagsToTest == 0 )
   if ( m_iCurrentTestIndex == m_iIndexFirstRadioFlagsTest )
   {
      for( int i=m_iCurrentTestIndex; i<m_iTestsCount; i++ )
         m_TestStepsInfo[m_iTestsCount].iDataRateToTest = m_iDataRateToApply;
      strcpy(m_szStatusMessage2, L("Testing modulation schemes parameters"));

      if ( m_iDataRateToApply > 0 )
      {
         m_iCurrentTestIndex = m_iTestsCount-1;
         _switch_to_step(NEGOCIATE_RADIO_STEP_END);
         return;
      }
   }
   _switch_to_step(m_iStep);
}

void MenuNegociateRadio::_compute_datarate_settings_to_apply()
{
   m_fQualityOfDataRateToApply = 0.0;
   m_iDataRateToApply = 0;
   m_iIndexTestDataRateToApply = -1;
   _computeQualities();

   int iTest6, iTest12, iTest18, iTest24, iTest48, iTestMCS0, iTestMCS1, iTestMCS2, iTestMCS3, iTestMCS4;
   float fQuality6 = _getComputedQualityForDatarate(6000000, &iTest6);
   float fQuality12 = _getComputedQualityForDatarate(12000000, &iTest12);
   float fQuality18 = _getComputedQualityForDatarate(18000000, &iTest18);
   float fQuality24 = _getComputedQualityForDatarate(24000000, &iTest24);
   float fQuality48 = _getComputedQualityForDatarate(48000000, &iTest48);
   float fQualityMCS0 = _getComputedQualityForDatarate(-1, &iTestMCS0);
   float fQualityMCS1 = _getComputedQualityForDatarate(-2, &iTestMCS1);
   float fQualityMCS2 = _getComputedQualityForDatarate(-3, &iTestMCS2);
   float fQualityMCS3 = _getComputedQualityForDatarate(-4, &iTestMCS3);
   float fQualityMCS4 = _getComputedQualityForDatarate(-5, &iTestMCS4);
   
   float fQualityMin6 = _getMinComputedQualityForDatarate(6000000, &iTest6);
   float fQualityMin12 = _getMinComputedQualityForDatarate(12000000, &iTest12);
   float fQualityMin18 = _getMinComputedQualityForDatarate(18000000, &iTest18);
   float fQualityMin24 = _getMinComputedQualityForDatarate(24000000, &iTest24);
   float fQualityMin48 = _getMinComputedQualityForDatarate(48000000, &iTest48);
   float fQualityMinMCS0 = _getMinComputedQualityForDatarate(-1, &iTestMCS0);
   float fQualityMinMCS1 = _getMinComputedQualityForDatarate(-2, &iTestMCS1);
   float fQualityMinMCS2 = _getMinComputedQualityForDatarate(-3, &iTestMCS2);
   float fQualityMinMCS3 = _getMinComputedQualityForDatarate(-4, &iTestMCS3);
   float fQualityMinMCS4 = _getMinComputedQualityForDatarate(-5, &iTestMCS4);

   /*
   if ( fQualityMCS4 > 0.99 )
   {
      m_fQualityOfDataRateToApply = fQualityMCS4;
      m_iIndexTestDataRateToApply = iTestMCS4;
      m_iDataRateToApply = -5;
   }
   else if ( fQualityMCS3 > 0.99 )
   {
      m_fQualityOfDataRateToApply = fQualityMCS3;
      m_iIndexTestDataRateToApply = iTestMCS3;
      m_iDataRateToApply = -4;
   }
   else 
   */
   if ( (fQualityMinMCS2 > 0.9) && (fQualityMCS2 >= fQuality18*0.98) )
   {
      m_fQualityOfDataRateToApply = fQualityMCS2;
      m_iIndexTestDataRateToApply = iTestMCS2;
      m_iDataRateToApply = -3;
   }
   /*
   else if ( fQuality48 > 0.99 )
   {
      m_fQualityOfDataRateToApply = fQuality48;
      m_iIndexTestDataRateToApply = iTest48;
      m_iDataRateToApply = 48000000;
   }
   else if ( fQuality24 > 0.99 )
   {
      m_fQualityOfDataRateToApply = fQuality24;
      m_iIndexTestDataRateToApply = iTest24;
      m_iDataRateToApply = 24000000;
   }
   */
   else
   {
      m_fQualityOfDataRateToApply = fQuality18;
      m_iIndexTestDataRateToApply = iTest18;
      m_iDataRateToApply = 18000000;
   }

   log_line("Computed Q: 6: %.3f 18: %.3f, 24: %.3f, 48: %.3f",
      fQuality6, fQuality18, fQuality24, fQuality48);
   log_line("Computed Q: MCS0: %.3f, MCS2: %.3f, MCS3: %.3f, MCS4: %.3f",
      fQualityMCS0, fQualityMCS2, fQualityMCS3, fQualityMCS4);
   log_line("Will apply datarate: %d (%u bps), test index: %d", m_iDataRateToApply, getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0), m_iIndexTestDataRateToApply);
}

void MenuNegociateRadio::_apply_new_settings()
{
   log_line("Applying new radio settings...");
   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("No model.");
      return;
   }
   _computeQualities();

   log_line("Appling datarate: %d (%u bps), test index: %d, quality: %.3f", m_iDataRateToApply, getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0), m_iIndexTestDataRateToApply, m_fQualityOfDataRateToApply);
   log_line("Current model radio flags (for radio link 1): %u (%s)", g_pCurrentModel->radioLinksParams.link_radio_flags[0], str_get_radio_frame_flags_description2(g_pCurrentModel->radioLinksParams.link_radio_flags[0]));

   m_uRadioFlagsToApply = g_pCurrentModel->radioLinksParams.link_radio_flags[0];

   float fBestFlagsQuality = 0.0;

   if ( m_iDataRateToApply < 0 )
   {
      for( int i=m_iIndexFirstRadioFlagsTest; i<m_iTestsCount; i++ )
      {
         float fQuality = m_TestStepsInfo[i].fComputedQuality;
         log_line("Quality of radio flags (%s): %.3f", str_get_radio_frame_flags_description2(m_TestStepsInfo[i].uFlagsToTest), fQuality);
         if ( fQuality > 0.5 )
         if ( fQuality > fBestFlagsQuality )
         {
            fBestFlagsQuality = fQuality;
            if ( fQuality >= m_fQualityOfDataRateToApply*0.92 )
            {
               log_line("New greater radio flags quality: %.3f", fQuality);
               m_fQualityOfDataRateToApply = fQuality;
               m_uRadioFlagsToApply &= ~(RADIO_FLAG_STBC_VEHICLE | RADIO_FLAG_LDPC_VEHICLE);
               m_uRadioFlagsToApply |= m_TestStepsInfo[i].uFlagsToTest;
               if ( m_uRadioFlagsToApply & RADIO_FLAG_LDPC_VEHICLE )
               if ( !(m_uRadioFlagsToApply & RADIO_FLAG_STBC_VEHICLE) )
                  m_uRadioFlagsToApply &= ~(RADIO_FLAG_LDPC_VEHICLE);
            }
         }
      }
   }

   log_line("Appling radio flags: %u (%s)", m_uRadioFlagsToApply, str_get_radio_frame_flags_description2(m_uRadioFlagsToApply));
   log_line("Max/Min video bitrate interval to set: %u / %u bps",
     (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 ) * DEFAULT_VIDEO_LINK_LOAD_PERCENT,
     (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 ) * 30 );

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( (g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps / 100) * DEFAULT_VIDEO_LINK_LOAD_PERCENT > getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) )
         g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps = (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 )* DEFAULT_VIDEO_LINK_LOAD_PERCENT;
   
      if ( (i == VIDEO_PROFILE_USER) || (i == VIDEO_PROFILE_HIGH_QUALITY) )
      if ( (g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps / 100) * 30 < getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) )
         g_pCurrentModel->video_link_profiles[i].bitrate_fixed_bps = (getRealDataRateFromRadioDataRate(m_iDataRateToApply, 0) / 100 ) * 30;
   }
   g_pCurrentModel->validate_radio_flags();
}

bool MenuNegociateRadio::periodicLoop()
{
   m_iLoopCounter++;
   if ( -1 == m_MenuIndexCancel )
   if ( g_TimeNow > m_uShowTime + 4000 )
   {
      m_MenuIndexCancel = addMenuItem(new MenuItem("Cancel", "Aborts the autoadjustment procedure without making any changes."));
      invalidate();
   }

   if ( m_bWaitingVehicleConfirmation )
   {
      if ( (g_TimeNow > m_uStepStartTime + 3000) && (!m_bWaitingCancelConfirmationFromUser) )
      {
         if ( m_iCurrentTestIndex < m_iIndexFirstRadioFlagsTest )
            m_iCountFailedSteps++;
         if ( (m_iStep == NEGOCIATE_RADIO_STEP_CANCEL) || (m_iStep == NEGOCIATE_RADIO_STEP_END) )
         {
            if ( m_iCountFailedSteps >= 6 )
            {
               menu_stack_pop(0);
               setModal(false);
               m_bWaitingCancelConfirmationFromUser = true;
               log_line("Timing out operation with 6 failed steps.");
               addMessage2(0, "Failed to negociate radio links.", "You radio links quality is very poor. Please fix the physical radio links quality and try again later.");
            }
            else
            {
               menu_stack_pop(0);
               setModal(false);
            }
            return false;
         }
         else
            _advance_to_next_step();
      }
      else if ( (g_TimeNow > m_uLastTimeSendCommandToVehicle + 100) && (!m_bWaitingCancelConfirmationFromUser) )
         _send_command_to_vehicle();
   }
   else
   {
      if ( m_iStep == NEGOCIATE_RADIO_STEP_DATA_RATE )
      {
         for(int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( !hardware_radio_index_is_wifi_radio(i) )
               continue;
            m_TestStepsInfo[m_iCurrentTestIndex].iRadioInterfacesRXPackets[i] = g_SM_RadioStats.radio_interfaces[i].totalRxPackets;
            m_TestStepsInfo[m_iCurrentTestIndex].iRadioInterfacesRxLostPackets[i] = g_SM_RadioStats.radio_interfaces[i].totalRxPacketsBad + g_SM_RadioStats.radio_interfaces[i].totalRxPacketsLost;
         }
         if ( g_TimeNow > m_uStepStartTime + 1200 )
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
      menu_stack_pop(0);
      setModal(false);
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
   u32 uParam1 = 0;
   u32 uParam2 = 0;
   int iParam1 = 0;
   memcpy(&uParam1, pPacketData + sizeof(t_packet_header) + 2*sizeof(u8), sizeof(u32));
   memcpy(&uParam2, pPacketData + sizeof(t_packet_header) + 2*sizeof(u8)+sizeof(u32), sizeof(u32));
   memcpy(&iParam1, &uParam1, sizeof(int));
   log_line("Received negociate radio link conf, command %d, param1: %d, param2: %u", uCommand, iParam1, uParam2);
      

   if ( uCommand != m_iStep )
      return;
   if ( (uCommand == NEGOCIATE_RADIO_STEP_END) || (uCommand == NEGOCIATE_RADIO_STEP_CANCEL  ) )
   {
      if ( uCommand == NEGOCIATE_RADIO_STEP_END )
      {
         log_line("Saving new video radio datarate on current model: %d, radio flags: %u (%s)", m_iDataRateToApply, m_uRadioFlagsToApply, str_get_radio_frame_flags_description2(m_uRadioFlagsToApply));
         g_pCurrentModel->resetVideoLinkProfilesToDataRates(m_iDataRateToApply, m_iDataRateToApply);
         g_pCurrentModel->radioLinksParams.link_radio_flags[0] = m_uRadioFlagsToApply | MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         g_bMustNegociateRadioLinksFlag = false;
      }
      if ( (uCommand == NEGOCIATE_RADIO_STEP_CANCEL) && (m_iCountFailedSteps >= 6) )
      {
         menu_stack_pop(0);
         setModal(false);
         m_bWaitingCancelConfirmationFromUser = true;
         log_line("Finishing up operation with 6 failed steps.");
         addMessage2(0, "Failed to negociate radio links.", "You radio links quality is very poor. Please fix the physical radio links quality and try again later.");
      }
      else
      {
         menu_stack_pop(0);
         setModal(false);
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
      menu_stack_pop(0);
      //setModal(false);
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
