#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"

typedef struct
{
   t_packet_header* pPH; // pointer inside pRawData
   t_packet_header_video_full_98* pPHVF; // pointer inside pRawData
   u8* pVideoData; // pointer inside pRawData
   u8* pRawData;
   u32 uReceivedTime;
   u32 uRequestedTime; // if requested for retransmission
   bool bEndOfFirstIFrameDetected;
   bool bEmpty;
   bool bOutputed;
}
type_rx_video_packet_info;

typedef struct
{
   type_rx_video_packet_info packets[MAX_TOTAL_PACKETS_IN_BLOCK];
   u32 uVideoBlockIndex;
   u32 uReceivedTime;
   int iBlockDataSize;
   int iBlockDataPackets;
   int iBlockECPackets;
   int iRecvDataPackets;
   int iRecvECPackets;
   int iReconstructedECUsed;
}
type_rx_video_block_info;


class VideoRxPacketsBuffer
{
   public:
      VideoRxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex);
      virtual ~VideoRxPacketsBuffer();

      bool init(Model* pModel);
      bool uninit();
      void emptyBuffers(const char* szReason);
      
      // Returns true if the packet has the highest video block/packet index received (in order)
      bool checkAddVideoPacket(u8* pPacket, int iPacketLength);

      u32 getMaxReceivedVideoBlockIndex();
      bool hasFirstVideoPacketInBuffer();
      bool hasIncompleteBlocks();
      int getBlocksCountInBuffer();
      type_rx_video_packet_info* getFirstVideoPacketInBuffer();
      type_rx_video_block_info* getFirstVideoBlockInBuffer();
      type_rx_video_block_info* getVideoBlockInBuffer(int iStartPosition);
      void advanceStartPosition();
      void advanceStartPositionToVideoBlock(u32 uVideoBlockIndex);

      bool isFrameEnded();
      u32 getLastFrameEndTime();
      bool isInsideIFrame();

   protected:

      bool _check_allocate_video_block_in_buffer(int iBufferIndex);
      void _empty_block_buffer_packet_index(int iBufferIndex, int iPacketIndex);
      void _empty_block_buffer_index(int iBufferIndex);
      void _empty_buffers(const char* szReason);
      void _check_do_ec_for_video_block(int iBufferIndex);
      // Returns true if the packet has the highest video block/packet index received (in order)
      bool _add_video_packet_to_buffer(int iBufferIndex, u8* pPacket, int iPacketLength);

      static int m_siVideoBuffersInstancesCount;
      bool m_bInitialized;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;
      u32 m_uMaxVideoBlockIndexInBuffer;
      bool m_bEndOfFirstIFrameDetected;
      bool m_bIsInsideIFrame;
      bool m_bFrameEnded;
      u32  m_uFrameEndedTime;

      int m_iBufferIndexFirstReceivedBlock;
      int m_iBufferIndexFirstReceivedPacketIndex;
      type_rx_video_block_info m_VideoBlocks[MAX_RXTX_BLOCKS_BUFFER];
      u8 m_TempVideoBuffer[MAX_PACKET_TOTAL_SIZE];

      u32 m_uMaxVideoBlockIndexReceived;
      u32 m_uMaxVideoBlockPacketIndexReceived;
};

