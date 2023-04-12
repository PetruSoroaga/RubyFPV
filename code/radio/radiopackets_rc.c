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
