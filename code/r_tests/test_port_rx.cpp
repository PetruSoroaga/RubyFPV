#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/encr.h"

#include <time.h>
#include <sys/resource.h>

bool quit = false;

bool bSummary = false;
bool bOnlyErrors = false;
   
int totalLost = 0;
u32 streamsLastPacketIndex[MAX_RADIO_STREAMS];
int streamsTotalLost[MAX_RADIO_STREAMS];
u32 lastVideoBlock = 0;
u32 uTotalPackets = 0;
u32 uTotalPacketsTypes[256];
u32 uSummaryLastUpdateTime = 0;
int iStreamId = -1;

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   quit = true;
} 

void process_packet(int iInterfaceIndex )
{
   //printf("x");
   fflush(stdout);

   radio_hw_info_t* pNICInfo = hardware_get_radio_info(iInterfaceIndex);
         
   int length = 0;
   u8* pBuffer = NULL;
   int bCRCOk = 0;
   int okPacket = 0;
   pBuffer = radio_process_wlan_data_in(iInterfaceIndex, &length); 
   if ( NULL == pBuffer )
   {
      printf("NULL receive buffer. Ignoring...");
      return;
   }

   int lastDBM = pNICInfo->monitor_interface_read.radioInfo.nDbm;
   int lastDataRate = pNICInfo->monitor_interface_read.radioInfo.nRate;

   u8* pData = pBuffer;
   int nLength = length;
   while ( nLength > 0 )
   { 
      t_packet_header* pPH = (t_packet_header*)pData;
      t_packet_header_video_full* pPHVF = NULL;
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA_FULL )
         pPHVF = (t_packet_header_video_full*) (pData+sizeof(t_packet_header));

      uTotalPackets++;
      uTotalPacketsTypes[pPH->packet_type]++;

      int expectedLength = pPH->total_length-pPH->total_headers_length;
      u32 packetIndex = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      u32 uStreamId = pPH->stream_packet_idx >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      u32 gap = 0;
      if ( -1 != iStreamId )
      if ( uStreamId != iStreamId )
      {
         printf("x");
         fflush(stdout);
         pData += pPH->total_length;
         nLength -= pPH->total_length;
         continue;
      }

      if ( ! bSummary )
         printf("\nReceived %d bytes, stream id: %d, packet index: %u", nLength, uStreamId, packetIndex);

      if ( streamsLastPacketIndex[uStreamId] != MAX_U32 )
         gap = packetIndex - streamsLastPacketIndex[uStreamId];
      if ( gap != 1 )
      {
         totalLost++;
         streamsTotalLost[uStreamId]++;
         if ( ! bSummary )
           printf("\n   Missing packets: %d [%u to %u]", gap-1, streamsLastPacketIndex[uStreamId], packetIndex );
      }

      u32 videogap = 0;
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA_FULL )
      if ( pPHVF->video_block_index > lastVideoBlock )
      {
         videogap = pPHVF->video_block_index - lastVideoBlock;
         if ( videogap > 1 )
         if ( ! bSummary )
            printf("\n   Missing packets: %d [%u to %u], video gap: %d [%u jump to %u]", gap-1, streamsLastPacketIndex[uStreamId], packetIndex, videogap-1, lastVideoBlock, pPHVF->video_block_index );
      }

      streamsLastPacketIndex[uStreamId] = packetIndex;
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA_FULL )
         lastVideoBlock = pPHVF->video_block_index;

      if ( bSummary )
      if ( get_current_timestamp_ms() >= uSummaryLastUpdateTime + 1000 )
      {
         uSummaryLastUpdateTime = get_current_timestamp_ms();
         printf("Total recv pckts: %u; ", uTotalPackets );
         for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         {
            if ( uTotalPacketsTypes[i] > 0 )
               printf(" %u of type %d, ", uTotalPacketsTypes[i], i);
            uTotalPacketsTypes[i] = 0;
         }
         uTotalPackets = 0;
         printf(" Total lost: %d", totalLost);
         for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         {
            if ( streamsTotalLost[i] > 0 )
               printf(", %u lost on stream %d", streamsTotalLost[i], i);
            uTotalPacketsTypes[i] = 0;
         }
         
         printf("\n");
      }

      if ( bOnlyErrors || bSummary )
      {
         pData += pPH->total_length;
         nLength -= pPH->total_length;
 
         continue;
      }

      char szPrefix[5];
      szPrefix[0] = 0;
      if ( nLength < 1000 )
         strcpy(szPrefix, " ");
      if ( nLength < 100 )
         strcpy(szPrefix, "  ");

      char szPrefix2[5];
      szPrefix2[0] = 0;
      if ( expectedLength < 1000 )
         strcpy(szPrefix2, " ");
      if ( expectedLength < 100 )
         strcpy(szPrefix2, "  ");
      if ( expectedLength < 10 )
         strcpy(szPrefix2, "   ");

      printf("\n   dbm: %d, dbm noise: %d, rate: %d, %s%d bytes, payload %s%d bytes, packet index: %u,  ", lastDBM, pNICInfo->monitor_interface_read.radioInfo.nDbmNoise, lastDataRate, szPrefix, nLength, szPrefix2, expectedLength, packetIndex);

      okPacket = packet_process_and_check(iInterfaceIndex, pData, nLength, &bCRCOk);

      printf("checks ok: %d, crc ok: %d,\t ", okPacket,  bCRCOk);
      if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
         printf("short CRC, %d bytes   ", pPH->total_headers_length-sizeof(u32) );
      else
         printf("full CRC, %d bytes\t", pPH->total_length-sizeof(u32) );
      printf("Component: %d, Type: %d, can TX: %d\n", pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->packet_type, (pPH->packet_flags&PACKET_FLAGS_BIT_CAN_START_TX)?1:0);

      pData += pPH->total_length;
      nLength -= pPH->total_length;
   }
}

int try_read_packets(int iInterfaceIndex)
{
   long miliSec = 500;
   fd_set readset;
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = miliSec*1000;
            
   int maxfd = -1;
   FD_ZERO(&readset);
   for(int i=0; i<hardware_get_radio_interfaces_count(); ++i)
   {
      if ( i != iInterfaceIndex )
         continue;
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( pNICInfo->openedForRead )
      {
         FD_SET(pNICInfo->monitor_interface_read.selectable_fd, &readset);
         if ( pNICInfo->monitor_interface_read.selectable_fd > maxfd )
            maxfd = pNICInfo->monitor_interface_read.selectable_fd;
      } 
   }

   int nResult = select(maxfd+1, &readset, NULL, NULL, &to);
   return nResult;
}

int main(int argc, char *argv[])
{
   if ( argc < 4 )
   {
      printf("\nYou must specify network, frequency and a port number:  test_port_rx wlan0 5745 15 [-errors]/[-summary]/[stream index]\n   14 - uplink , 15 - downlink\n");
      printf("\n-errors : show only errors and missing packets\n");
      return -1;
   }
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   char* szCard = argv[1];
   int iFreq = atoi(argv[2]);
   int port = atoi(argv[3]);
   
   if ( strcmp(argv[argc-1], "-errors") == 0 )
      bOnlyErrors = true;
   else if ( strcmp(argv[argc-1], "-summary") == 0 )
      bSummary = true;
   else if ( argc >= 5 )
      iStreamId = atoi(argv[argc-1]);

   log_init("RX_TEST_PORT");
   log_enable_stdout();
   hardware_enumerate_radio_interfaces();
   int iInterfaceIndex = -1;
   for( int i=0; i<hardware_get_radio_interfaces_count(); ++i)
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( 0 == strcmp(szCard, pNICInfo->szName ) )
      {
         //launch_set_frequency(i, freq, NULL);
       iInterfaceIndex = i;
       break;
      }
   }
   hardware_sleep_ms(100);

   radio_init_link_structures();
   radio_enable_crc_gen(1);

   if ( -1 == iInterfaceIndex )
   {
      printf("\nCan't find interface %s\n", szCard);
      return 0;
   }

   launch_set_frequency(NULL, iInterfaceIndex, iFreq, NULL);

   printf("\nStart receiving on lan: %s (interface %d), %d Mhz, port: %d\n", szCard, iInterfaceIndex+1, iFreq, port);

   int pcapR =  radio_open_interface_for_read(iInterfaceIndex, port);
   if ( pcapR < 0 )
   {
      printf("\nFailed to open port.\n");
      return -1;
   }

   int pcapW = radio_open_interface_for_write(iInterfaceIndex);

   printf("Opened wlan interface for read and write.\n");

   //FILE* fd = fopen("out.bin", "w");
   
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      streamsLastPacketIndex[i] = MAX_U32;
      streamsTotalLost[i] = 0;
   }

   for( int i=0; i<256; i++ )
      uTotalPacketsTypes[i] = 0;

   if ( -1 == iStreamId )
      printf("\nLooking for any stream\n");
   else
      printf("\nLooking only for stream id %d\n", iStreamId);

   if ( ! bSummary )
      printf("\nWaiting for data ...");

   while (!quit)
   {
   
      //fflush(stdout);
      
      //hardware_sleep_ms(2);

      int iRepeatCount = 100;
      while ( iRepeatCount > 0 )
      {
         iRepeatCount--;

         int nResult = try_read_packets(iInterfaceIndex);

         if( nResult <= 0 )
         {
            if ( ! bSummary )
            if ( nResult == 0 )
               printf(".");
            else
               printf("x");
            iRepeatCount = 0;
            continue;
         }

         process_packet(iInterfaceIndex);
      }

   }
   //fclose(fd);
   radio_close_interface_for_write(iInterfaceIndex);
   radio_close_interface_for_read(iInterfaceIndex);
   printf("\nFinised receiving test.\n");
   return (0);
}
