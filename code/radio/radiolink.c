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

#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include "../base/base.h"
#include "../base/encr.h"
#include "../base/hw_procs.h"
#include "../common/string_utils.h"
#include "radiotap.h"
#include <time.h>
#include <sys/resource.h>
#include "radiolink.h"
#include "radiopackets2.h"


int _radio_encode_port(int port)
{
   return 0;
}

void radio_init_link_structures()
{
}

void radio_link_cleanup()
{
   
}

void radio_reset_packets_default_frequencies(int iRCEnabled)
{
   
}

void radio_enable_crc_gen(int enable)
{
}


void radio_set_debug_flag()
{
   
}

// Returns 0 if the packet can't be sent (right now or ever)

int radio_can_send_packet_on_slow_link(int iLinkId, int iPacketType, int iFromController, u32 uTimeNow)
{
   
   return 0;
}

int radio_get_last_second_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow)
{
   return 0;
}

int radio_get_last_500ms_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow)
{
   return 0;
}

int radio_set_out_datarate(int rate_bps)
{
   return 0;
}

void radio_set_frames_flags(u32 frameFlags)
{
   
}


u32 radio_get_received_frames_type()
{
   return 0;
}

int radio_interfaces_broken()
{
   return 0;
}

int radio_get_last_read_error_code()
{
   return 0;
}

int radio_open_interface_for_read(int interfaceIndex, int portNumber)
{
   return 0;
}


int radio_open_interface_for_write(int interfaceIndex)
{
  return 0;
}

void radio_close_interface_for_read(int interfaceIndex)
{
   
}

void radio_close_interfaces_for_read()
{
  
}

void radio_close_interface_for_write(int interfaceIndex)
{
   
}


u8* radio_process_wlan_data_in(int interfaceNumber, int* outPacketLength)
{
   return NULL;
}

// returns 0 for failure, total length of packet for success

int packet_process_and_check(int interfaceNb, u8* pPacketBuffer, int iBufferLength, int* pbCRCOk)
{
  return 0;
}

int get_last_processing_error_code()
{
   return 0;
}

u32 radio_get_next_radio_link_packet_index(int iLocalRadioLinkId)
{
   return 0;
}

int radio_build_new_raw_packet(int iLocalRadioLinkId, u8* pRawPacket, u8* pPacketData, int nInputLength, int portNb, int bEncrypt, int iExtraData, u8* pExtraData)
{
  return 0;
}


int radio_write_raw_packet(int interfaceIndex, u8* pData, int dataLength)
{
 
   return 0;
}

// Returns the number of bytes written or -1 for error, -2 for write error

int radio_write_sik_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow)
{

   return 0
}
