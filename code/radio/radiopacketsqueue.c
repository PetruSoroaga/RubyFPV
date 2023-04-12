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
#include "radiopacketsqueue.h"
#include "radiopackets2.h"
#include "radiolink.h"


void packets_queue_init(t_packet_queue* pQueue)
{
   if ( NULL == pQueue )
      return;
   pQueue->queue_start_pos = -1;
   pQueue->queue_end_pos = -1;   
}

int packets_queue_is_empty(t_packet_queue* pQueue)
{
   if ( NULL == pQueue )
      return 1;
   return (pQueue->queue_start_pos == pQueue->queue_end_pos);
}

int packets_queue_has_packets(t_packet_queue* pQueue)
{
   if ( NULL == pQueue )
      return 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return 0;

   if ( pQueue->queue_end_pos > pQueue->queue_start_pos )
      return pQueue->queue_end_pos - pQueue->queue_start_pos;

   return pQueue->queue_end_pos + (MAX_PACKETS_IN_QUEUE - pQueue->queue_start_pos);
}

int packets_queue_inject_packet_first(t_packet_queue* pQueue, u8* pBuffer)
{
   if ( NULL == pQueue )
      return 0;

   t_packet_header* pPH = (t_packet_header*)pBuffer;


   if ( (-1 == pQueue->queue_start_pos) || (pQueue->queue_start_pos == pQueue->queue_end_pos) )
   {
      pQueue->queue_start_pos = 0;
      pQueue->queue_end_pos = 1;
      pQueue->timeFirstPacket = get_current_timestamp_ms();
      pQueue->packets_queue[0].packet_length = pPH->total_length;
      memcpy( &(pQueue->packets_queue[0].packet_buffer[0]), pBuffer, pQueue->packets_queue[0].packet_length);
      return 1;
   }

   pQueue->queue_start_pos--;
   if ( pQueue->queue_start_pos < 0 )
      pQueue->queue_start_pos = MAX_PACKETS_IN_QUEUE-1;

   pQueue->packets_queue[pQueue->queue_start_pos].packet_length = pPH->total_length;
   memcpy( &(pQueue->packets_queue[pQueue->queue_start_pos].packet_buffer[0]), pBuffer, pQueue->packets_queue[pQueue->queue_start_pos].packet_length);
   return 1;
}

int packets_queue_add_packet(t_packet_queue* pQueue, u8* pBuffer)
{
   return packets_queue_add_packet2(pQueue, pBuffer, -1, 0);
}

int packets_queue_add_packet2(t_packet_queue* pQueue, u8* pBuffer, int length, int has_radio_header)
{
   if ( NULL == pQueue )
      return 0;
   if ( -1 == pQueue->queue_start_pos )
   {
      pQueue->queue_start_pos = 0;
      pQueue->queue_end_pos = 0;
   }

   // Full queue
   if ( ((pQueue->queue_end_pos+1) % MAX_PACKETS_IN_QUEUE) == pQueue->queue_start_pos )
      return 0;

   // Empty queue
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      pQueue->timeFirstPacket = get_current_timestamp_ms();

   pQueue->packets_queue[pQueue->queue_end_pos].has_radio_header = (u8)has_radio_header;
   pQueue->packets_queue[pQueue->queue_end_pos].packet_length = (u16)length;
   if ( -1 == length )
   {
      t_packet_header* pPH = (t_packet_header*)pBuffer;
      pQueue->packets_queue[pQueue->queue_end_pos].packet_length = pPH->total_length;
   }

   memcpy( &(pQueue->packets_queue[pQueue->queue_end_pos].packet_buffer[0]), pBuffer, pQueue->packets_queue[pQueue->queue_end_pos].packet_length);

   pQueue->queue_end_pos++;
   if ( pQueue->queue_end_pos >= MAX_PACKETS_IN_QUEUE )
      pQueue->queue_end_pos = 0;

   return 1;
}

u8* packets_queue_pop_packet(t_packet_queue* pQueue, int* pLength)
{
   if ( NULL == pQueue )
      return NULL;
   if ( NULL != pLength )
      *pLength = 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return NULL;

   if ( NULL != pLength )
      *pLength = pQueue->packets_queue[pQueue->queue_start_pos].packet_length;
   u8* pRet = &(pQueue->packets_queue[pQueue->queue_start_pos].packet_buffer[0]);

   pQueue->queue_start_pos++;
   if ( pQueue->queue_start_pos >= MAX_PACKETS_IN_QUEUE )
      pQueue->queue_start_pos = 0;

   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      pQueue->timeFirstPacket = MAX_U32;

   return pRet;
}

u8* packets_queue_peek_packet(t_packet_queue* pQueue, int index, int* pLength)
{
   if ( NULL == pQueue )
      return NULL;
   if ( NULL != pLength )
      *pLength = 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return NULL;

   int iPos = pQueue->queue_start_pos + index;
   if ( iPos >= MAX_PACKETS_IN_QUEUE )
      iPos -= MAX_PACKETS_IN_QUEUE;
   if ( NULL != pLength )
      *pLength = pQueue->packets_queue[iPos].packet_length;
   u8* pRet = &(pQueue->packets_queue[iPos].packet_buffer[0]);

   return pRet;
}
