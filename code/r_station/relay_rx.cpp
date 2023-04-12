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
#include "../base/config.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"

#include "relay_rx.h"
#include "processor_rx_video.h"
#include "shared_vars.h"
#include "timers.h"

int relay_rx_process_single_received_packet(int iRadioInterfaceIndex, u8* pPacketData, int iPacketLength)
{
   if ( (NULL == pPacketData) || (iPacketLength <= 0) )
      return -1;

   return 0;
}