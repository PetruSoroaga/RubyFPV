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

#include "radiopackets_rc.h"


void packet_header_rc_full_set_rc_channel_value(t_packet_header_rc_full_frame_upstream* pphrc, u16 ch, u16 val)
{
   if ( ch >= MAX_RC_CHANNELS )
      return;
   if ( NULL == pphrc )
      return;
   pphrc->ch_lowBits[ch] = val & 0xFF;
   if ( ch & 0x0001 )
      pphrc->ch_highBits[ch>>1] = ((pphrc->ch_highBits[ch>>1]) & 0x0F) | (((val>>8) & 0x0F)<<4);
   else
      pphrc->ch_highBits[ch>>1] = ((pphrc->ch_highBits[ch>>1]) & 0xF0) | ((val>>8) & 0x0F);
}

u16 packet_header_rc_full_get_rc_channel_value(t_packet_header_rc_full_frame_upstream* pphrc, u16 ch)
{
   if ( ch >= MAX_RC_CHANNELS )
      return 700;
   if ( NULL == pphrc )
      return 700;

   if ( ch & 0x0001 )
      return ((pphrc->ch_lowBits[ch]) | (((pphrc->ch_highBits[ch>>1]) & 0xF0)<<4));
   else
      return ((pphrc->ch_lowBits[ch]) | (((pphrc->ch_highBits[ch>>1]) & 0x0F)<<8));
}
