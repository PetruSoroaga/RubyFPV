/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "radio_links.h"
#include "../base/hardware_radio.h"
#include "../base/radio_utils.h"
#include "shared_vars.h"
#include "timers.h"


bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParams)
{
   if ( (NULL == pModel) || (NULL == pRadioLinkParams) )
      return false;
   if ( (iRadioLink < 0) || (iRadioLink >= pModel->radioLinksParams.links_count) )
      return false;

   // Update HT20/HT40 if needed
     
   if ( (pRadioLinkParamsOld->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE) != 
        (pRadioLinkParams->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE) )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         if ( iRadioLink != pModel->radioInterfacesParams.interface_link_id[i] )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         radio_utils_set_interface_frequency(pModel, i, iRadioLink, pModel->radioLinksParams.link_frequency_khz[iRadioLink], g_pProcessStats, 0);
      }
   }

   // Apply data rates

   // If downlink data rate for an Atheros card has changed, update it.
      
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( ((pRadioHWInfo->typeAndDriver & 0xFF) != RADIO_TYPE_ATHEROS) &&
        ((pRadioHWInfo->typeAndDriver & 0xFF) != RADIO_TYPE_RALINK) )
         continue;

      int nRateTx = pRadioLinkParams->link_datarate_video_bps[iRadioLink];
      update_atheros_card_datarate(pModel, i, nRateTx, g_pProcessStats);
      g_TimeNow = get_current_timestamp_ms();
   }

   // Radio flags are applied on the fly, when sending each radio packet
   
   return true;
}
