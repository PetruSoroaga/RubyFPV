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

#include "../base/base.h"
#include "radiopackets_short.h"
#include "radiolink.h"

u8 s_uRadioPacketsShortIndexes[MAX_RADIO_INTERFACES];

void radio_packets_short_init()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_uRadioPacketsShortIndexes[i] = 0;
}

void radio_packet_short_init(t_packet_header_short* pPHS)
{
   if ( NULL == pPHS )
      return;
   pPHS->start_header = SHORT_PACKET_START_BYTE_REG_PACKET;
   pPHS->crc = 0;
   pPHS->data_length = 0;
   pPHS->packet_id = 0;
   pPHS->last_ack_packet_id = 0;
}

u8 radio_packets_short_get_next_id_for_radio_interface(int iInterfaceIndex)
{
   u8 uIndex = s_uRadioPacketsShortIndexes[iInterfaceIndex];
   s_uRadioPacketsShortIndexes[iInterfaceIndex]++;
   return uIndex; 
}

int radio_buffer_is_valid_short_packet(u8* pBuffer, int iLength)
{
   if ( (NULL == pBuffer) || (iLength < sizeof(t_packet_header_short)) )
      return 0;

   if ( ((*pBuffer) != SHORT_PACKET_START_BYTE_REG_PACKET) &&
        ((*pBuffer) != SHORT_PACKET_START_BYTE_START_PACKET) &&
        ((*pBuffer) != SHORT_PACKET_START_BYTE_END_PACKET) )
      return 0;

   t_packet_header_short* pPHS = (t_packet_header_short*)pBuffer;
      
   if ( pPHS->data_length > iLength - sizeof(t_packet_header_short) )
      return 0;

   u8 crc = base_compute_crc8(pBuffer+2, pPHS->data_length + sizeof(t_packet_header_short)-2);
   if ( crc != pPHS->crc )
   {
      //log_buffer1(pBuffer, pPHS->data_length + sizeof(t_packet_header_short), 6);
      return 0;
   }
   return 1;
}
