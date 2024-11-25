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
}
type_tx_video_packet_info;


class VideoTxPacketsBuffer
{
   public:
      VideoTxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex);
      virtual ~VideoTxPacketsBuffer();

      bool init(Model* pModel);
      bool uninit();
      void discardBuffer();
      void updateVideoHeader(Model* pModel);
      void updateCurrentKFValue();
      void fillVideoPackets(u8* pVideoData, int iDataSize, bool bEndOfFrame, bool bIsInsideIFrame);
      void addNewVideoPacket(u8* pVideoData, int iDataSize, bool bEndOfFrame, bool bIsInsideIFrame);
      int hasPendingPacketsToSend();
      int sendAvailablePackets(int iMaxCountToSend);
      void resendVideoPacket(u32 uRetransmissionId, u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex);

   protected:

      void _checkAllocatePacket(int iBufferIndex, int iPacketIndex);
      void _fillVideoPacketHeaders(int iBufferIndex, int iPacketIndex, int iVideoSize, bool bEndOfFrame, bool bIsInsideIFrame);
      void _sendPacket(int iBufferIndex, int iPacketIndex, u32 uRetransmissionId);
      static int m_siVideoBuffersInstancesCount;
      bool m_bInitialized;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;
      int m_iVideoStreamInfoIndex;

      u32 m_uNextVideoBlockIndexToGenerate;
      u32 m_uNextVideoBlockPacketIndexToGenerate;
      u32 m_uNextBlockPacketSize;
      u32 m_uNextBlockDataPackets;
      u32 m_uNextBlockECPackets;
      t_packet_header m_PacketHeader;
      t_packet_header_video_full_98 m_PacketHeaderVideo;

      int m_iNextBufferIndexToFill;
      int m_iNextBufferPacketIndexToFill;
      int m_iCurrentBufferIndexToSend;
      int m_iCurrentBufferPacketIndexToSend;
      u8 m_TempVideoBuffer[MAX_PACKET_TOTAL_SIZE];
      int m_iTempVideoBufferFilledBytes;
      type_tx_video_packet_info m_VideoPackets[MAX_RXTX_BLOCKS_BUFFER][MAX_TOTAL_PACKETS_IN_BLOCK];
      int m_iCountReadyToSend;

      u32 m_uRadioStreamPacketIndex;
};

