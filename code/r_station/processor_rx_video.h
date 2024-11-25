#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/shared_mem_controller_only.h"
#include "video_rx_buffers.h"

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
#define RX_PACKET_STATE_RECEIVED 0x01
#define RX_PACKET_STATE_OUTPUTED 0x02

typedef struct
{
   u32 uState;
   int video_data_length;
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
   int video_data_length;
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
      ProcessorRxVideo(u32 uVehicleId, u8 uVideoStreamIndex);
      virtual ~ProcessorRxVideo();

      static void oneTimeInit();
      static ProcessorRxVideo* getVideoProcessorForVehicleId(u32 uVehicleId, u32 uVideoStreamIndex);
      //static void log(const char* format, ...);

      virtual bool init();
      virtual bool uninit();
      virtual void resetState();
      void onControllerSettingsChanged();

      void pauseProcessing();
      void resumeProcessing();

      u32 getLastTimeVideoStreamChanged();
      u32 getLastestVideoPacketReceiveTime();
      int getCurrentlyReceivedVideoProfile();
      int getCurrentlyReceivedVideoFPS();
      int getCurrentlyReceivedVideoKeyframe();
      int getCurrentlyMissingVideoPackets();
      int getVideoWidth();
      int getVideoHeight();
      int getVideoFPS();
      int getVideoType();
      shared_mem_video_stream_stats_history* getVideoDecodeStatsHistory();
      
      void updateHistoryStats(u32 uTimeNow);
      virtual void periodicLoop(u32 uTimeNow);
      virtual int handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length);

      static int m_siInstancesCount;
      static FILE* m_fdLogFile;

      u32 m_uVehicleId;
      u8 m_uVideoStreamIndex;
      int m_iIndexVideoDecodeStats;
      u32 m_uLatestVideoPacketReceiveTime;
      VideoRxPacketsBuffer* m_pVideoRxBuffer;

   protected:
      void resetReceiveState();
      void resetOutputState();
      void resetReceiveBuffers(int iToMaxIndex);
      void resetReceiveBuffersBlock(int iBlockIndex);

      void updateVideoDecodingStats(u8* pRadioPacket, int iPacketLength);
      
      void reconstructBlock(int rx_buffer_block_index);

      void checkAndRequestMissingPackets();
      void checkAndDiscardBlocksTooOld();
      void sendPacketToOutput(int rx_buffer_block_index, int block_packet_index);
      void pushIncompleteBlocksOut(int iStackIndexToDiscardTo, bool bTooOld);
      void pushFirstBlockOut();

      int preProcessRetransmittedVideoPacket(int interfaceNb, u8* pBuffer, int length);
      int preProcessReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length);

      int processRetransmittedVideoPacket(u8* pBuffer, int length);
      int processReceivedVideoPacket(u8* pBuffer, int length);
      
      int onNewReceivedValidVideoPacket(Model* pModel, u8* pBuffer, int length, int iAddedToStackIndex);

      bool m_bInitialized;
      int m_iInstanceIndex;
      bool m_bPaused;
      
      // Configuration

      u32 m_uRetryRetransmissionAfterTimeoutMiliseconds;
      int m_iMilisecondsMaxRetransmissionWindow;
      u32 m_uTimeIntervalMsForRequestingRetransmissions;

      // Output state

      u32 m_uLastOutputVideoBlockTime;
      u32 m_uLastOutputVideoBlockIndex;
      u32 m_uLastOutputVideoBlockPacketIndex;
      u32 m_uLastOutputVideoBlockDataPackets;

      // Rx state 

      shared_mem_video_stream_stats_history m_SM_VideoDecodeStatsHistory;
      
      type_last_rx_packet_info m_InfoLastReceivedVideoPacket;
      u8 m_uLastReceivedVideoLinkProfile;
      u32 m_uLastHardEncodingsChangeVideoBlockIndex;
      u32 m_uLastVideoResolutionChangeVideoBlockIndex;

      u32 m_uLastTimeRequestedRetransmission;
      u32 m_uRequestRetransmissionUniqueId;

      u32 m_uEncodingsChangeCount;
      u32 m_uTimeLastVideoStreamChanged;

      u32 m_TimeLastHistoryStatsUpdate;
      u32 m_TimeLastRetransmissionsStatsUpdate;

      u32 m_uTimeLastReceivedNewVideoPacket;
      u32 m_uLastBlockReceivedAckKeyframeInterval;
      u32 m_uLastBlockReceivedAdaptiveVideoInterval;
      u32 m_uLastBlockReceivedSetVideoBitrate;
      u32 m_uLastBlockReceivedEncodingExtraFlags2;
};
