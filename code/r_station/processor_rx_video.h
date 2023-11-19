#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/shared_mem_controller_only.h"

#define MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH 20

typedef struct
{
   u32 uRetransmissionTimeMinim; // in miliseconds
   u32 uRetransmissionTimeAverage;
   u32 uRetransmissionTimeLast;
   u32 retransmissionTimePreviousBuffer[MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH];
   u32 uRetransmissionTimePreviousIndex;
   u32 uRetransmissionTimePreviousSum;
   u32 uRetransmissionTimePreviousSumCount;
}
type_retransmission_stats;

typedef struct
{
   u32 receive_time;
   u32 stream_packet_idx;
   u32 video_block_index;
   u32 video_block_packet_index;
}
type_last_rx_packet_info;

#define RX_PACKET_STATE_EMPTY 0
#define RX_PACKET_STATE_RECEIVED 1

typedef struct
{
   int state;
   int packet_length;
   u8 uRetrySentCount;
   u32 uTimeFirstRetrySent;
   u32 uTimeLastRetrySent;
   u8* pData;
}
type_received_block_packet_info;

typedef struct
{
   u32 video_block_index; // MAX_U32 if it's empty
   int packet_length;
   int data_packets;
   int fec_packets;
   int received_data_packets;
   int received_fec_packets;

   int totalPacketsRequested;
   u32 uTimeFirstPacketReceived;
   u32 uTimeFirstRetrySent;
   u32 uTimeLastRetrySent;
   u32 uTimeLastUpdated; //0 for none
   type_received_block_packet_info packetsInfo[MAX_TOTAL_PACKETS_IN_BLOCK];

} type_received_block_info;


class ProcessorRxVideo
{
   public:
      ProcessorRxVideo(u32 uVehicleId, u32 uVideoStreamIndex);
      virtual ~ProcessorRxVideo();

      static void oneTimeInit();
      //static void log(const char* format, ...);

      bool init();
      bool uninit();
      void resetState();
      void resetRetransmissionsStats();
      void onControllerSettingsChanged();

      u32 getLastTimeVideoStreamChanged();
      int getCurrentlyReceivedVideoProfile();
      int getCurrentlyReceivedVideoFPS();
      int getCurrentlyReceivedVideoKeyframe();
      int getCurrentlyMissingVideoPackets();
      int getVideoWidth();
      int getVideoHeight();
      int getVideoFPS();
      shared_mem_video_stream_stats* getVideoDecodeStats();
      shared_mem_video_stream_stats_history* getVideoDecodeStatsHistory();
      shared_mem_controller_retransmissions_stats* getControllerRetransmissionsStats();

      void periodicLoop(u32 uTimeNow);

      int handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length);

      static int m_siInstancesCount;
      static FILE* m_fdLogFile;

      u32 m_uVehicleId;
      u32 m_uVideoStreamIndex;

   protected:
      void resetReceiveState();
      void resetOutputState();
      void resetReceiveBuffers();
      void resetReceiveBuffersBlock(int iBlockIndex);

      void logCurrentRxBuffers(bool bIncludeRetransmissions);
      void updateRetransmissionsHistoryStats(u32 uTimeNow);
      void updateHistoryStatsDiscaredAllStack();
      void updateHistoryStatsDiscaredStackSegment(int countDiscardedBlocks);
      void updateHistoryStatsBlockOutputed(int rx_buffer_block_index, bool hasRetransmittedPackets);

      void reconstructBlock(int rx_buffer_block_index);

      void discardRetransmissionsRequestsTooOld();
      void checkAndRequestMissingPackets();
      void checkAndDiscardBlocksTooOld();
      void sendPacketToOutput(int rx_buffer_block_index, int block_packet_index);
      void pushIncompleteBlocksOut(int countToPush, bool bTooOld);
      void pushFirstBlockOut();

      void addPacketToReceivedBlocksBuffers(u8* pBuffer, int length, int rx_buffer_block_index, bool bWasRetransmitted);

      int preProcessRetransmittedVideoPacket(int interfaceNb, u8* pBuffer, int length);
      int preProcessReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length);

      int processRetransmittedVideoPacket(u8* pBuffer, int length);
      int processReceivedVideoPacket(u8* pBuffer, int length);
      
      static bool m_sbFECInitialized;
      bool m_bInitialized;
      int m_iInstanceIndex;

      // Configuration

      u32 m_uRetryRetransmissionAfterTimeoutMiliseconds;
      int m_iMilisecondsMaxRetransmissionWindow;
      u32 m_uTimeIntervalMsForRequestingRetransmissions;
      bool m_bUseNewVideoPacketsStructure;

      // Output state

      u32 m_uLastOutputVideoBlockTime;
      u32 m_uLastOutputVideoBlockIndex;

      // Rx state 

      shared_mem_video_stream_stats m_SM_VideoDecodeStats;
      shared_mem_video_stream_stats_history m_SM_VideoDecodeStatsHistory;
      shared_mem_controller_retransmissions_stats m_SM_RetransmissionsStats;

      // Video blocks are stored in right expected order in the stack (based on video block index)

      type_received_block_info* m_pRXBlocksStack[MAX_RXTX_BLOCKS_BUFFER];
      int m_iRXBlocksStackTopIndex;
      int m_iRXMaxBlocksToBuffer;

      type_last_rx_packet_info m_LastReceivedVideoPacketInfo;
      u8 m_uLastReceivedVideoLinkProfile;
      u32 m_uLastHardEncodingsChangeVideoBlockIndex;
      u32 m_uLastVideoResolutionChangeVideoBlockIndex;

      u32 m_uLastTimeRequestedRetransmission;
      u32 m_uRequestRetransmissionUniqueId;

      u32 m_uEncodingsChangeCount;
      u32 m_uTimeLastVideoStreamChanged;

      u32 m_TimeLastVideoStatsUpdate;
      u32 m_TimeLastHistoryStatsUpdate;
      u32 m_TimeLastRetransmissionsStatsUpdate;

      u32 m_uLastBlockReceivedAckKeyframeInterval;
      u32 m_uLastBlockReceivedAdaptiveVideoInterval;
      u32 m_uLastBlockReceivedSetVideoBitrate;
      u32 m_uLastBlockReceivedEncodingExtraFlags2;
};

