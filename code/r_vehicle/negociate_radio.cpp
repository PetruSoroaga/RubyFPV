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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/utils.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "adaptive_video.h"
#include "shared_vars.h"
#include "timers.h"
#include "negociate_radio.h"
#include "packets_utils.h"

bool s_bIsNegociatingRadioLinks = false;
u32 s_uTemporaryRadioFlagsBeforeNegociateRadio = 0;
int s_iTemporaryDatarateBeforeNegociateRadio = 0;
u32 s_uTimeStartOfNegociatingRadioLinks = 0;
u32 s_uTimeLastNegociateRadioLinksReceivedCommand = 0;

void _negociate_radio_link_end(bool bApply, int iDatarate, u32 uRadioFlags)
{
   if ( ! s_bIsNegociatingRadioLinks )
      return;

   log_line("[NegociateRadioLink] Ending. Apply: %s, datarate: %d, radio flags: %u",
       bApply?"yes":"no", iDatarate, uRadioFlags);
   adaptive_video_set_temporary_bitrate(0);
   packet_utils_set_adaptive_video_datarate(s_iTemporaryDatarateBeforeNegociateRadio);
   radio_remove_temporary_frames_flags();
   radio_set_frames_flags(s_uTemporaryRadioFlagsBeforeNegociateRadio);
   s_uTimeStartOfNegociatingRadioLinks = 0;
   s_uTimeLastNegociateRadioLinksReceivedCommand = 0;
   s_bIsNegociatingRadioLinks = false;

   log_line("[NegociateRadioLink] Ended negiciation.");

   if ( bApply )
   {
      log_line("[NegociateRadioLink] Apply final negociated params to model.");
      g_pCurrentModel->resetVideoLinkProfilesToDataRates(iDatarate, iDatarate);
      g_pCurrentModel->radioLinksParams.link_radio_flags[0] = uRadioFlags;
      g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
      g_pCurrentModel->validate_radio_flags();
      saveCurrentModel();

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_GENERIC<<8);
      PH.total_length = sizeof(t_packet_header);

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
            
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }
}

int negociate_radio_process_received_radio_link_messages(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return 0;

   u8 uCommand = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];
   u32 uParam1 = 0;
   u32 uParam2 = 0;
   int iParam1 = 0;
   memcpy(&uParam1, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8), sizeof(u32));
   memcpy(&uParam2, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32), sizeof(u32));
   memcpy(&iParam1, &uParam1, sizeof(int));
   s_uTimeLastNegociateRadioLinksReceivedCommand = g_TimeNow;
   log_line("[NegociateRadioLink] Received command %d, datarate: %d, radio flags: %s = %s", uCommand, iParam1, str_format_binary_number(uParam2), str_get_radio_frame_flags_description2(uParam2));
   
   if ( uCommand == NEGOCIATE_RADIO_STEP_END )
   {
       log_line("[NegociateRadioLink] Received command end: set video HQ/HP data rates to: %d/%d", iParam1, iParam1);
       log_line("[NegociateRadioLink] Received command end: set radio frame flags to %s = %s", str_format_binary_number(uParam2), str_get_radio_frame_flags_description2(uParam2));
       _negociate_radio_link_end(true, iParam1, uParam2);
   }
   else if ( uCommand == NEGOCIATE_RADIO_STEP_CANCEL )
   {
      log_line("[NegociateRadioLink] Received command cancel.");
      _negociate_radio_link_end(false, 0, 0);
   }
   else if ( uCommand == NEGOCIATE_RADIO_STEP_DATA_RATE )
   {
      log_line("[NegociateRadioLink] Received command set datarate: %d, radio flags: %s = %s", iParam1, str_format_binary_number(uParam2), str_get_radio_frame_flags_description2(uParam2));
      if ( ! s_bIsNegociatingRadioLinks )
      {
         log_line("[NegociateRadioLink] Started negociation.");
         s_uTimeStartOfNegociatingRadioLinks = g_TimeNow;
         s_uTemporaryRadioFlagsBeforeNegociateRadio = radio_get_current_frames_flags();
         adaptive_video_set_temporary_bitrate(DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE);
         s_iTemporaryDatarateBeforeNegociateRadio = packet_utils_get_last_set_adaptive_video_datarate();
         log_line("[NegociateRadioLink] On start negociation: current adaptive datarate: %d, current radio flags: %s = %s", s_iTemporaryDatarateBeforeNegociateRadio, str_format_binary_number(s_uTemporaryRadioFlagsBeforeNegociateRadio), str_get_radio_frame_flags_description2(s_uTemporaryRadioFlagsBeforeNegociateRadio) );
      }
      s_bIsNegociatingRadioLinks = true;
   
      packet_utils_set_adaptive_video_datarate(iParam1);
      radio_set_temporary_frames_flags(uParam2);
   }
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8) + 2*sizeof(u32);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   packet[sizeof(t_packet_header)] = 1;
   packet[sizeof(t_packet_header)+sizeof(u8)] = uCommand;
   memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), &uParam1, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8)+sizeof(u32), &uParam2, sizeof(u32));
   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
   return 0;
}

void negociate_radio_periodic_loop()
{
   if ( (! s_bIsNegociatingRadioLinks) || (0 == s_uTimeStartOfNegociatingRadioLinks) || (0 == s_uTimeLastNegociateRadioLinksReceivedCommand) )
      return;

   if ( (g_TimeNow > s_uTimeStartOfNegociatingRadioLinks + 60*2*1000) || (g_TimeNow > s_uTimeLastNegociateRadioLinksReceivedCommand + 12000) )
   {
      log_line("[NegociateRadioLink] Trigger end due to timeout (no progress received from controller).");
      _negociate_radio_link_end(false, 0, 0);
   }
}

bool negociate_radio_link_is_in_progress()
{
   return s_bIsNegociatingRadioLinks;
}

