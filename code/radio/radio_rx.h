#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"


#define MAX_RX_PACKETS_QUEUE 1000

typedef struct
{
   u8* pPacketsBuffers[MAX_RX_PACKETS_QUEUE];
   int iPacketsLengths[MAX_RX_PACKETS_QUEUE];
   int iPacketsAreShort[MAX_RX_PACKETS_QUEUE];
   int iPacketsRxInterface[MAX_RX_PACKETS_QUEUE];
   int iCurrentRxPacketIndex;
   int iCurrentRxPacketToConsume;

   int iRadioInterfacesBroken[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxTimeouts[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxBadPackets[MAX_RADIO_INTERFACES];

   u32 uTimeLastStatsUpdate;
   u32 uVehiclesIds[MAX_CONCURENT_VEHICLES];
   u32 uTotalRxPackets[MAX_CONCURENT_VEHICLES];
   u32 uTotalRxPacketsBad[MAX_CONCURENT_VEHICLES];
   u32 uTotalRxPacketsLost[MAX_CONCURENT_VEHICLES];
   u32 uTmpRxPackets[MAX_CONCURENT_VEHICLES];
   u32 uTmpRxPacketsBad[MAX_CONCURENT_VEHICLES];
   u32 uTmpRxPacketsLost[MAX_CONCURENT_VEHICLES];
   int iMaxRxPacketsPerSec[MAX_CONCURENT_VEHICLES];
   int iMinRxPacketsPerSec[MAX_CONCURENT_VEHICLES];
   u32 uLastRxRadioLinkPacketIndex[MAX_CONCURENT_VEHICLES][MAX_RADIO_INTERFACES]; // per vehicle, per radio interface
   u32 uTimeLastMinute;
   int iMaxPacketsInQueue;
   int iMaxPacketsInQueueLastMinute;

   u32 uMaxLoopTime;
} __attribute__((packed)) t_radio_rx_buffers;


#ifdef __cplusplus
extern "C" {
#endif

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, shared_mem_radio_stats_interfaces_rx_graph* pSMRadioRxGraphs, int iSearchMode);
void radio_rx_stop_rx_thread();

void radio_rx_set_timeout_interval(int iMiliSec);

void radio_rx_pause_interface(int iInterfaceIndex);
void radio_rx_resume_interface(int iInterfaceIndex);
void radio_rx_mark_quit();

int radio_rx_any_interface_broken();
int radio_rx_any_interface_bad_packets();
int radio_rx_get_interface_bad_packets_error_and_reset(int iInterfaceIndex);
int radio_rx_any_rx_timeouts();
int radio_rx_get_timeout_count_and_reset(int iInterfaceIndex);
void radio_rx_reset_interfaces_broken_state();
t_radio_rx_buffers* radio_rx_get_state();

int radio_rx_has_packets_to_consume();
u8* radio_rx_get_next_received_packet(int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex);

u32 radio_rx_get_and_reset_max_loop_time();
u32 radio_rx_get_and_reset_max_loop_time_read();
u32 radio_rx_get_and_reset_max_loop_time_queue();

#ifdef __cplusplus
}  
#endif
