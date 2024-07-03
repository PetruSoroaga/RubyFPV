#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"

#if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA_ZERO3)
#define MAX_RX_PACKETS_QUEUE 500
#else
#define MAX_RX_PACKETS_QUEUE 50
#endif
typedef struct
{
   u32 uVehicleId;
   u32 uDetectedFirmwareType;
   u32 uTotalRxPackets;
   u32 uTotalRxPacketsBad;
   u32 uTotalRxPacketsLost;
   u32 uTmpRxPackets;
   u32 uTmpRxPacketsBad;
   u32 uTmpRxPacketsLost;
   int iMaxRxPacketsPerSec;
   int iMinRxPacketsPerSec;
   u32 uLastRxRadioLinkPacketIndex[MAX_RADIO_INTERFACES]; // per radio interface

} __attribute__((packed)) t_radio_rx_state_vehicle;

typedef struct
{
   u8* pPacketsBuffers[MAX_RX_PACKETS_QUEUE];
   int iPacketsLengths[MAX_RX_PACKETS_QUEUE];
   int iPacketsAreShort[MAX_RX_PACKETS_QUEUE];
   int iPacketsRxInterface[MAX_RX_PACKETS_QUEUE];
   int iCurrentRxPacketIndex; // Where next packet will be added
   int iCurrentRxPacketToConsume; // Where the first packet to read/consume is

   int iRadioInterfacesBroken[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxTimeouts[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxBadPackets[MAX_RADIO_INTERFACES];

   u32 uTimeLastStatsUpdate;
   t_radio_rx_state_vehicle vehicles[MAX_CONCURENT_VEHICLES];

   u32 uMaxLoopTime;
   u32 uAcceptedFirmwareType;
   u32 uTimeLastMinute;
   int iMaxPacketsInQueue;
   int iMaxPacketsInQueueLastMinute;
} __attribute__((packed)) t_radio_rx_state;

typedef struct
{
   u8* pPacketData;
   int iPacketLength;
   int iPacketIsShort;
   int iPacketRxInterface;
} __attribute__((packed)) type_received_radio_packet;

#ifdef __cplusplus
extern "C" {
#endif

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, shared_mem_radio_stats_interfaces_rx_graph* pSMRadioRxGraphs, int iSearchMode, u32 uAcceptedFirmwareType);
void radio_rx_stop_rx_thread();

void radio_rx_set_custom_thread_priority(int iPriority);
void radio_rx_set_timeout_interval(int iMiliSec);

void radio_rx_pause_interface(int iInterfaceIndex, const char* szReason);
void radio_rx_resume_interface(int iInterfaceIndex);
void radio_rx_mark_quit();
void radio_rx_set_dev_mode();

int radio_rx_detect_firmware_type_from_packet(u8* pPacketBuffer, int nPacketLength);

int radio_rx_any_interface_broken();
int radio_rx_any_interface_bad_packets();
int radio_rx_get_interface_bad_packets_error_and_reset(int iInterfaceIndex);
int radio_rx_any_rx_timeouts();
int radio_rx_get_timeout_count_and_reset(int iInterfaceIndex);
void radio_rx_reset_interfaces_broken_state();
t_radio_rx_state* radio_rx_get_state();

int radio_rx_has_retransmissions_requests_to_consume();
int radio_rx_has_packets_to_consume();
u8* radio_rx_get_next_received_packet(int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex);
int radio_rx_get_received_packets(int iCount, type_received_radio_packet* pOutputArray);

u32 radio_rx_get_and_reset_max_loop_time();
u32 radio_rx_get_and_reset_max_loop_time_read();
u32 radio_rx_get_and_reset_max_loop_time_queue();

#ifdef __cplusplus
}  
#endif
