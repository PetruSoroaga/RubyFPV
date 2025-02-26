#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/parser_h264.h"
#include "../radio/radiopackets2.h"

//  [packet header][video segment header][video seg header important][video data][000]
//  | pPH          | pPHVS               | pPHVSImp                  |pActualVideoData
//  | pRawData ptr                       | pVideoData ptr
//                                       [  <- video block packet size ->            ]
//                                                                   [-vid size-]

typedef struct
{
   u8* pRawData;
   u8* pVideoData; // pointer inside pRawData
   t_packet_header* pPH; // pointer inside pRawData
   t_packet_header_video_segment* pPHVS; // pointer inside pRawData
   t_packet_header_video_segment_important* pPHVSImp; // pointer inside pRawData
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
      void fillVideoPacketsFromCSI(u8* pVideoData, int iDataSize, bool bEndOfFrame);
      bool fillVideoPacketsFromRTSPPacket(u8* pVideoRawData, int iRawDataSize, bool bSingle, bool bEnd, u32 uNALType);
      int hasPendingPacketsToSend();
      int sendAvailablePackets(int iMaxCountToSend);
      void resendVideoPacket(u32 uRetransmissionId, u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex);

      u32 getCurrentOutputFrameIndex();
      u32 getCurrentOutputNALIndex();
      int getCurrentUsableRawVideoDataSize();
      bool getResetOverflowFlag();
      int getCurrentMaxUsableRawVideoDataSize();

   protected:

      void _checkAllocatePacket(int iBufferIndex, int iPacketIndex);
      void _fillVideoPacketHeaders(int iBufferIndex, int iPacketIndex, bool bIsECPacket, int iRawVideoDataSize, u32 uNALPresenceFlags, bool bEndOfTransmissionFrame);
      void _addNewVideoPacket(u8* pRawVideoData, int iRawVideoDataSize, u32 uNALPresenceFlags, bool bEndOfTransmissionFrame);
      void _sendPacket(int iBufferIndex, int iPacketIndex, u32 uRetransmissionId);
      static int m_siVideoBuffersInstancesCount;
      bool m_bInitialized;
      bool m_bOverflowFlag;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;
      int m_iVideoStreamInfoIndex;
      ParserH264 m_ParserInputH264;

      u16 m_uCurrentH264FrameIndex;
      u16 m_uCurrentH264NALIndex;
      u32 m_uPreviousParsedNAL;
      u32 m_uCurrenltyParsedNAL;
      u32 m_uNextVideoBlockIndexToGenerate;
      u32 m_uNextVideoBlockPacketIndexToGenerate;
      u32 m_uNextBlockPacketSize;
      u32 m_uNextBlockDataPackets;
      u32 m_uNextBlockECPackets;
      t_packet_header m_PacketHeader;
      t_packet_header_video_segment m_PacketHeaderVideo;
      t_packet_header_video_segment_important m_PacketHeaderVideoImportant;
      t_packet_header_video_segment* m_pLastPacketHeaderVideoFilldedIn;
      t_packet_header_video_segment_important* m_pLastPacketHeaderVideoImportantFilledIn;
      int m_iUsableRawVideoDataSize;

      int m_iNextBufferIndexToFill;
      int m_iNextBufferPacketIndexToFill;
      int m_iCurrentBufferIndexToSend;
      int m_iCurrentBufferPacketIndexToSend;
      u8 m_TempVideoBuffer[MAX_PACKET_TOTAL_SIZE];
      int m_iTempVideoBufferFilledBytes;
      u32 m_uTempBufferNALPresenceFlags;
      type_tx_video_packet_info m_VideoPackets[MAX_RXTX_BLOCKS_BUFFER][MAX_TOTAL_PACKETS_IN_BLOCK];
      int m_iCountReadyToSend;

      u32 m_uRadioStreamPacketIndex;
};

