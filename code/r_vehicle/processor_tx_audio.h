#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"

class ProcessorTxAudio
{
   public:
      ProcessorTxAudio();
      virtual ~ProcessorTxAudio();

      void init(Model* pModel);
      void resetState(Model* pModel);
      u32 getAverageAudioInputBps();

      int openAudioStream();
      void closeAudioStream();
      bool isAudioStreamOpened();

      int startLocalRecording();
      int stopLocalRecording();
      
      int tryReadAudioInputStream();
      int sendAudioPackets();

   protected:
      void _localRecordBuffer(u8* pBuffer, int iLength);
      void _sendAudioPacket(u8* pBuffer, int iLength, u32 uAudioPacketIndex);

      int m_iAudioStream;
      FILE* m_fAudioRecordingFile;
      bool m_bLocalRecording;
      int m_iRecordingFileNumber;
      int m_iSchemePacketSize;
      int m_iSchemeDataPackets;
      int m_iSchemeECPackets;

      u32 m_uTimeLastTryReadAudioInputStream;

      int m_iBreakStampMatchPosition;
      char m_szBreakStamp[24];

      u32 m_StatsAudioInputComputedBps;
      u32 m_StatsTmpAudioInputReadBytes;
      u32 m_StatsTimeLastComputeAudioInputBps;

      u8 m_ListBufferedInputPackets[MAX_BUFFERED_AUDIO_PACKETS][MAX_PACKET_PAYLOAD];
      u8 m_ListBufferedInputECPackets[10][MAX_PACKET_PAYLOAD];
      int m_iCurrentInputReadPacketIndex;
      int m_iCurrentInputReadPacketPosition;
      
      int m_iCurrentInputBufferPacketToSend;
      u32 m_uCurrentTxAudioBlockIndex;
      u32 m_uCurrentTxAudioSegmentIndex;
};

