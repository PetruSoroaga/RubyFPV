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

#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/shared_mem.h"
#include "../radio/radiolink.h"

#include <time.h>
#include <sys/resource.h>

t_packet_header_ruby_telemetry_extended* s_pPHRTE = NULL;

void exit_with_value(int success)
{
   radio_close_interfaces_for_read();
   shared_mem_ruby_telemetry_close(s_pPHRTE);
   printf("%d\n", success);
   exit(success);
}

void process_data_ruby_telemetry_extended(u8* pBuffer, int length)
{
   memcpy((u8*)s_pPHRTE, pBuffer+sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended) );
   exit_with_value(1);
}

int main(int argc, char *argv[])
{
   log_disable();

   if ( argc < 2 )
   {
      //log_line("Too few params. Add wlan interfaces");
      printf("0\n");
      exit(0);
   }
   hw_set_priority_current_proc(-9); 

   hardware_enumerate_radio_interfaces();

   int i = 1;
   while( i < argc && i <= MAX_RADIO_INTERFACES+1) 
   {
      if ( argv[i][0] == '-' )
      {
         i++;
         continue;
      } 
      if ( i != 0 )
         hardware_sleep_ms(40); // wait a bit between configuring interfaces to reduce Atheros and Pi USB flakiness
      radio_open_interface_for_read(hardware_get_radio_index_by_name(argv[i]), RADIO_PORT_ROUTER_DOWNLINK );
      i++;
   } 

   s_pPHRTE = shared_mem_ruby_telemetry_open_for_write();
   packet_header_ruby_telemetry_extended_reset(s_pPHRTE);

   // Try for 0.2 sec(s) to find data

   u32 timeStart = get_current_timestamp_ms();
   u32 timeToWait = 200;

   while ( true )
   {
      if ( get_current_timestamp_ms() > timeStart + timeToWait )
         break;
      fd_set readset;
      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 10000;
      int maxfd = -1;
      FD_ZERO(&readset);
      for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( pNICInfo->openedForRead )
         {
            FD_SET(pNICInfo->monitor_interface_read.selectable_fd, &readset);
            if ( pNICInfo->monitor_interface_read.selectable_fd > maxfd )
               maxfd = pNICInfo->monitor_interface_read.selectable_fd;
         }
      }
      int n = select(maxfd+1, &readset, NULL, NULL, &to);
      if(n == 0)
         continue;

      for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);         
         if( ! FD_ISSET(pNICInfo->monitor_interface_read.selectable_fd, &readset) )
            continue;
         int bufferLength = 0;
         int bCRCOk = 0;
         u8* pBuffer = radio_process_wlan_data_in(i, &bufferLength);
         if ( pBuffer == NULL )
            continue;

         u8* pData = pBuffer;
         int nLength = bufferLength;
         while ( nLength > 0 )
         { 
            t_packet_header* pPH = (t_packet_header*)pData;
            int nPacketLength = packet_process_and_check(i, pData, nLength, &bCRCOk);
            if ( nPacketLength <= 0  )
            {
               //printf("\nFailed check\n");
               continue;
            }
            if ( ! bCRCOk )
            {
               //printf("\nFailed CRC\n");
               continue;
            }

            timeToWait = 3000;

            //if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) !=  PACKET_COMPONENT_VIDEO )
            //   printf("\nRecv packet type: %d-%d, len: %d / %d\n", pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->packet_type, pPH->total_length, length);

            if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
               process_data_ruby_telemetry_extended(pBuffer, nPacketLength);

            pData += pPH->total_length;
            nLength -= pPH->total_length;
         }
      }
   }
   //log_line("Vehicle not found");
   exit_with_value(0);
}
