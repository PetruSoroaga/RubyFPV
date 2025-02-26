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

#include "process_radio_out_packets.h"
#include "../base/hardware.h"
#include "../base/radio_utils.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../radio/radiolink.h"
#include "../radio/radio_duplicate_det.h"
#include "../radio/radio_tx.h"
#include "ruby_rt_station.h"
#include "shared_vars.h"
#include "timers.h"

void preprocess_radio_out_packet(u8* pPacketBuffer, int iPacketLength)
{
   if ( (NULL == pPacketBuffer) || (iPacketLength <= 0) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      
   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
      u8 uVehicleLinkId = *(pPacketBuffer + sizeof(t_packet_header));
      u8 uCommandId = *(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8));
      log_line("Received message to send to vehicle to configure SiK vehicle radio link %d, command: %d", (int) uVehicleLinkId+1, (int)uCommandId);
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      t_packet_header_command* pCom = (t_packet_header_command*)(pPacketBuffer + sizeof(t_packet_header));
      type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(pPH->vehicle_id_dest);
      if ( NULL != pRuntimeInfo )
      {
         pRuntimeInfo->uTimeLastCommandIdSent = g_TimeNow;
         pRuntimeInfo->uLastCommandIdSent = pCom->command_counter;
         pRuntimeInfo->uLastCommandIdRetrySent = pCom->command_resend_counter;
      }
      if ( pPH->packet_type == PACKET_TYPE_COMMAND )
      {
         t_packet_header_command* pPHC = (t_packet_header_command*)(pPacketBuffer + sizeof(t_packet_header));
         if ( ((pPHC->command_type & COMMAND_TYPE_MASK) == COMMAND_ID_SET_VIDEO_PARAMS) ||
              ((pPHC->command_type & COMMAND_TYPE_MASK) == COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES) ||
              ((pPHC->command_type & COMMAND_TYPE_MASK) == COMMAND_ID_RESET_VIDEO_LINK_PROFILE) ||
              ((pPHC->command_type & COMMAND_TYPE_MASK) == COMMAND_ID_GET_CORE_PLUGINS_INFO)||
              ((pPHC->command_type & COMMAND_TYPE_MASK) == COMMAND_ID_GET_ALL_PARAMS_ZIP) )
         {
            log_line("Send command. Reset H264 stream detected profile and level for VID %u", pPH->vehicle_id_dest);
            shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pPH->vehicle_id_dest); 
            if ( NULL != pSMVideoStreamInfo )
            {
               pSMVideoStreamInfo->uDetectedH264Profile = 0;
               pSMVideoStreamInfo->uDetectedH264ProfileConstrains = 0;
               pSMVideoStreamInfo->uDetectedH264Level = 0;
            }
            g_TimeLastVideoParametersOrProfileChanged = g_TimeNow;
         }
      }
   }

   if ( pPH->packet_type == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
   {
      u8 uCommand = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];
      if ( (uCommand == NEGOCIATE_RADIO_STEP_END) || (uCommand == NEGOCIATE_RADIO_STEP_CANCEL) )
      {
         g_bNegociatingRadioLinks = false;
         g_uTimeEndedNegiciateRadioLink = g_TimeNow;
      }
      else
         g_bNegociatingRadioLinks = true;
   }
}