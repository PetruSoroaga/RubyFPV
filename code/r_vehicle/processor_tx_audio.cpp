/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hw_procs.h"
#include "processor_tx_audio.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"

FILE* s_pFileRawStream = NULL;

u8* p_ec_audio_packets[MAX_DATA_PACKETS_IN_BLOCK];
u8* p_ec_audio_ecs[MAX_FECS_PACKETS_IN_BLOCK];

ProcessorTxAudio::ProcessorTxAudio()
{
   m_iAudioStream = -1;
   m_fAudioRecordingFile = NULL;
   m_bLocalRecording = false;
   m_uTimeLastTryReadAudioInputStream = 0;
   m_StatsAudioInputComputedBps = 0;
   m_StatsTmpAudioInputReadBytes = 0;
   m_StatsTimeLastComputeAudioInputBps = 0;
   
   m_iBreakStampMatchPosition = 0;
   m_iCurrentInputReadPacketIndex = 0;
   m_iCurrentInputReadPacketPosition = 0;

   m_iCurrentInputBufferPacketToSend = 0;
   m_uCurrentTxAudioBlockIndex = 0;
   m_uCurrentTxAudioSegmentIndex = 0;

   m_iSchemePacketSize = MAX_PACKET_PAYLOAD - 100;
   m_uSchemeDataPackets = 4;
   m_uSchemeECPackets = 2;

   strcpy(m_szBreakStamp, "0123456789");
   m_szBreakStamp[10] = 10;
   m_szBreakStamp[11] = 0;
}

ProcessorTxAudio::~ProcessorTxAudio()
{
   stopLocalRecording();
   closeAudioStream();
}

u32 ProcessorTxAudio::getAverageAudioInputBps()
{
   return m_StatsAudioInputComputedBps;
}

int ProcessorTxAudio::openAudioStream()
{
   if ( m_iAudioStream >= 0 )
      close(m_iAudioStream);
   if ( NULL != m_fAudioRecordingFile )
      fclose(m_fAudioRecordingFile);

   m_iAudioStream = -1;
   m_fAudioRecordingFile = NULL;
   m_StatsTmpAudioInputReadBytes = 0;

   m_iBreakStampMatchPosition = 0;
   m_iCurrentInputReadPacketIndex = 0;
   m_iCurrentInputReadPacketPosition = 0;

   m_iCurrentInputBufferPacketToSend = 0;
   m_uCurrentTxAudioBlockIndex = 0;
   m_uCurrentTxAudioSegmentIndex = 0;

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[Audio-Tx] Invalid params to open audio stream. Null model.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.enabled )
   {
      log_line("[Audio-Tx] Audio is disabled on vehicle. No audio stream to open.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      log_line("[Audio-Tx] No audio devices on vehicle. No audio stream to open.");
      return 0;
   }
  
   log_line("[Audio-Tx] Opening audio input stream: %s", FIFO_RUBY_AUDIO1);
   m_iAudioStream = open(FIFO_RUBY_AUDIO1, O_RDONLY);
   if ( m_iAudioStream < 0 )
   {
      log_softerror_and_alarm("[Audio-TX] Failed to open audio input stream: %s", FIFO_RUBY_AUDIO1);
      return 0;
   }
   log_line("[Audio-TX] Opened audio input stream: %s successfully. fd = %d", FIFO_RUBY_AUDIO1, m_iAudioStream);

   m_uSchemeDataPackets = g_pCurrentModel->audio_params.flags & 0xFF;
   m_uSchemeECPackets = (g_pCurrentModel->audio_params.flags >> 8 ) & 0xFF;
   if ( m_uSchemeDataPackets >= MAX_BUFFERED_AUDIO_PACKETS )
      m_uSchemeDataPackets = MAX_BUFFERED_AUDIO_PACKETS - 1;
   if ( m_uSchemeECPackets > 10 )
      m_uSchemeECPackets = 10;
   log_line("[Audio-Tx] Current EC scheme: data/EC: %u/%u, packet size: %d bytes",
       m_uSchemeDataPackets, m_uSchemeECPackets, m_iSchemePacketSize);

   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   startLocalRecording();
   #endif

   return 1;
}

void ProcessorTxAudio::closeAudioStream()
{
   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   stopLocalRecording();
   #endif

   if ( -1 != m_iAudioStream )
   {
      log_line("[Audio-Tx] Closed audio input stream %s", FIFO_RUBY_AUDIO1);
      close(m_iAudioStream);
   }
   m_iAudioStream = -1;

   m_StatsAudioInputComputedBps = 0;
}

bool ProcessorTxAudio::isAudioStreamOpened()
{
   if ( m_iAudioStream != -1 )
      return true;
   return false;
}


int ProcessorTxAudio::startLocalRecording()
{
   if ( m_bLocalRecording )
      return 0;

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[Audio-Tx] Invalid params to start recording. Null model.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.enabled )
   {
      log_line("[Audio-Tx] Audio is disabled on vehicle. No audio stream to record.");
      return 0;
   }
   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      log_line("[Audio-Tx] No audio devices on vehicle. No audio stream to record.");
      return 0;
   }

   m_bLocalRecording = true;
   m_iBreakStampMatchPosition = 0;
   
#ifdef FEATURE_LOCAL_AUDIO_RECORDING
   log_line("[Audio-Tx] Local recording started. Opening local recording output file...");
   m_iRecordingFileNumber = 0;
   if ( NULL != m_fAudioRecordingFile )
      fclose(m_fAudioRecordingFile);

   char szBuff[128];
   sprintf(szBuff, "%s%s%d", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, m_iRecordingFileNumber);
   m_fAudioRecordingFile = fopen(szBuff, "wb");
   if ( NULL == m_fAudioRecordingFile )
   {
      log_softerror_and_alarm("[Audio-Tx] Failed to open output recording file %s", szBuff);
      return 0;
   }
   else
      log_line("[Audio-Tx] Opened audio recording output file %s, fd:%d", szBuff, fileno(m_fAudioRecordingFile));
#endif

   //s_pFileRawStream = fopen("raw_audio_out_stream.data", "wb");

   return 1;
}

int ProcessorTxAudio::stopLocalRecording()
{
   if ( ! m_bLocalRecording )
      return 0;

   m_bLocalRecording = false;

#ifdef FEATURE_LOCAL_AUDIO_RECORDING
   log_line("[Audio-Tx] Local recording stopped. Closing local recording output file...");
   if ( NULL != m_fAudioRecordingFile )
      fclose(m_fAudioRecordingFile);
   m_fAudioRecordingFile = NULL;

   char szBuff[256];
   for( int i=0; i<5; i++ )
   {
      m_iRecordingFileNumber++;
      sprintf(szBuff, "rm -rf %s%s%d 2>/dev/null", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, m_iRecordingFileNumber);
      hw_execute_bash_command(szBuff, NULL);
   }
   m_iRecordingFileNumber = 0;
#endif

   if ( NULL != s_pFileRawStream )
      fclose(s_pFileRawStream);
   s_pFileRawStream = NULL;

   return 0;
}

int ProcessorTxAudio::tryReadAudioInputStream()
{
   if ( -1 == m_iAudioStream )
      return 0;


   // Try to read only every 10 ms (100 times/sec)

   if ( g_TimeNow < m_uTimeLastTryReadAudioInputStream + 10 )
      return 0;
   
   m_uTimeLastTryReadAudioInputStream = g_TimeNow;

   fd_set readset;
   u8 buffer[MAX_PACKET_PAYLOAD];
   
   FD_ZERO(&readset);
   FD_SET(m_iAudioStream, &readset);

   struct timeval timePipeInput;
   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 100; // 0.1 miliseconds timeout

   int selectResult = select(m_iAudioStream+1, &readset, NULL, NULL, &timePipeInput);
   if ( selectResult <= 0 )
      return 0;

   if( 0 == FD_ISSET(m_iAudioStream, &readset) )
      return 0;

   int countRead = read(m_iAudioStream, buffer, m_iSchemePacketSize);
   if ( countRead < 0 )
   {
      log_error_and_alarm("[Audio-Tx] Failed to read from audio input fifo: %s, returned code: %d, error: %s", FIFO_RUBY_AUDIO1, countRead, strerror(errno));
      return -1;
   }

   if ( countRead == 0 )
      return 0;

   m_StatsTmpAudioInputReadBytes += countRead;

   if ( g_TimeNow >= m_StatsTimeLastComputeAudioInputBps+1000 )
   {
      m_StatsTimeLastComputeAudioInputBps = g_TimeNow;
      m_StatsAudioInputComputedBps = m_StatsTmpAudioInputReadBytes * 8;
      m_StatsTmpAudioInputReadBytes = 0;
   }

#ifdef FEATURE_LOCAL_AUDIO_RECORDING
   if ( m_bLocalRecording )
      _localRecordBuffer(buffer, countRead);
#endif

   while ( countRead > 0 )
   {
      // Still room in the current input packet ?
      if ( m_iCurrentInputReadPacketPosition + countRead <= m_iSchemePacketSize )
      {
         memcpy(&m_ListBufferedInputPackets[m_iCurrentInputReadPacketIndex][m_iCurrentInputReadPacketPosition], buffer, countRead );
         m_iCurrentInputReadPacketPosition += countRead;
         countRead = 0;
         break;
      }

      int iAvailableRoom = m_iSchemePacketSize - m_iCurrentInputReadPacketPosition;
      memcpy(&m_ListBufferedInputPackets[m_iCurrentInputReadPacketIndex][m_iCurrentInputReadPacketPosition], buffer, iAvailableRoom );
      m_iCurrentInputReadPacketPosition = 0;
      m_iCurrentInputReadPacketIndex++;
      if ( m_iCurrentInputReadPacketIndex >= MAX_BUFFERED_AUDIO_PACKETS )
         m_iCurrentInputReadPacketIndex = 0;

      countRead -= iAvailableRoom;
      for( int i=0; i<=countRead; i++ )
         buffer[i] = buffer[i+iAvailableRoom];
   }
   return 1;
}

void ProcessorTxAudio::_localRecordBuffer(u8* pBuffer, int iLength)
{
   if ( (NULL == pBuffer) || (iLength <= 0) || (! m_bLocalRecording))
      return;

   bool bFoundBreakStamp = false;
   int iBreakFoundPosition = -1;
   u8* pTmp = pBuffer;
   for( int i=0; i<iLength; i++ )
   {
      if ( (*pTmp) != m_szBreakStamp[m_iBreakStampMatchPosition] )
      {
         pTmp++;
         m_iBreakStampMatchPosition = 0;
         continue;
      }
      pTmp++;
      m_iBreakStampMatchPosition++;
      if ( m_szBreakStamp[m_iBreakStampMatchPosition] == 0 )
      {
         iBreakFoundPosition = i+1;
         bFoundBreakStamp = true;
         break;
      }
   }

   if ( ! bFoundBreakStamp )
   {
      if ( NULL != m_fAudioRecordingFile )
      {
         int iWrite = fwrite(pBuffer, 1, iLength, m_fAudioRecordingFile);
         if ( iWrite != iLength )
            log_softerror_and_alarm("[Audio-Tx] Failed to write to output recording file. Write %d of %d bytes.", iWrite, iLength);
      }
      return;
   }
     

   if ( NULL != m_fAudioRecordingFile )
   {
      if ( iBreakFoundPosition > 11 )
      { 
         int iWrite = fwrite(pBuffer, 1, iBreakFoundPosition-11, m_fAudioRecordingFile);
         if ( iWrite != iBreakFoundPosition-11 )
            log_softerror_and_alarm("[Audio-Tx] Failed to write to output recording file. Write %d of %d bytes.", iWrite, iBreakFoundPosition);
      }
      fclose(m_fAudioRecordingFile);
   }

   m_iRecordingFileNumber++;
   char szBuff[256];
   sprintf(szBuff, "%s%s%d", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, m_iRecordingFileNumber);

   m_fAudioRecordingFile = fopen(szBuff, "wb");
   if ( NULL == m_fAudioRecordingFile )
   {
      log_softerror_and_alarm("[Audio-Tx] Failed to open output recording file %s", szBuff);
      return;
   }

   log_line("[Audio-Tx] Opened audio recording output file %s, fd:%d", szBuff, fileno(m_fAudioRecordingFile));

   int toWrite = iLength - iBreakFoundPosition;
   if ( toWrite > 0 )
   {
      int iWrite = fwrite(&pBuffer[iBreakFoundPosition], 1, toWrite, m_fAudioRecordingFile);
      if ( iWrite != toWrite )
         log_softerror_and_alarm("[Audio-Tx] Failed to write to output recording file. Write %d of %d bytes.", iWrite, toWrite);
   }
}

void ProcessorTxAudio::_sendAudioPacket(u8* pBuffer, int iLength, u32 uAudioPacketIndex)
{
   if ( (NULL == g_pCurrentModel) || (NULL == pBuffer) || ( iLength < m_iSchemePacketSize) )
      return;
   
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_AUDIO, PACKET_TYPE_AUDIO_SEGMENT, STREAM_ID_VIDEO_1);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + iLength;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&uAudioPacketIndex, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), pBuffer, iLength);

   send_packet_to_radio_interfaces(packet, PH.total_length, -1);

   if ( NULL != s_pFileRawStream )
      fwrite(packet, 1, PH.total_length, s_pFileRawStream);
}

// Returns number of packets sent (no matter if they where data or EC packets )

int ProcessorTxAudio::sendAudioPackets()
{
   if ( (m_iAudioStream < 0) || (NULL == g_pCurrentModel) )
      return 0;
   
   // No full packet read from input?
   if ( m_iCurrentInputBufferPacketToSend == m_iCurrentInputReadPacketIndex )
      return 0;

   int iPacketsReadyToSend = m_iCurrentInputReadPacketIndex - m_iCurrentInputBufferPacketToSend;
   if ( m_iCurrentInputReadPacketIndex < m_iCurrentInputBufferPacketToSend )
       iPacketsReadyToSend = MAX_BUFFERED_AUDIO_PACKETS - m_iCurrentInputBufferPacketToSend + m_iCurrentInputReadPacketIndex;

   if ( iPacketsReadyToSend <= 0 )
      return 0;

   if ( iPacketsReadyToSend > 5 )
      iPacketsReadyToSend = 5;
      
   int iCountPacketsSent = 0;

   for( int i=0; i<iPacketsReadyToSend; i++ )
   {
      if ( iCountPacketsSent >= 5 )
         break;

      u32 uAudioPacketIndex = ((m_uCurrentTxAudioBlockIndex & 0xFFFFFF) << 8) | m_uCurrentTxAudioSegmentIndex;
      _sendAudioPacket(m_ListBufferedInputPackets[m_iCurrentInputBufferPacketToSend], m_iSchemePacketSize, uAudioPacketIndex);
      
      iCountPacketsSent++;
      m_iCurrentInputBufferPacketToSend++;
      if ( m_iCurrentInputBufferPacketToSend >= MAX_BUFFERED_AUDIO_PACKETS )
         m_iCurrentInputBufferPacketToSend = 0;

      m_uCurrentTxAudioSegmentIndex++;

      // Must send EC packets now?
      if ( m_uCurrentTxAudioSegmentIndex >= m_uSchemeDataPackets )
      if ( m_uSchemeECPackets > 0 )
      {
         for( u32 u=0; u<m_uSchemeDataPackets; u++ )
         {
            int iIndex = m_iCurrentInputBufferPacketToSend - (int)m_uSchemeDataPackets;
            if ( iIndex < 0 )
               iIndex += MAX_BUFFERED_AUDIO_PACKETS;
            p_ec_audio_packets[u] = &m_ListBufferedInputPackets[iIndex][0];
         }

         for( u32 u=0; u<m_uSchemeECPackets; u++ )
            p_ec_audio_ecs[u] = &m_ListBufferedInputECPackets[u][0];

         fec_encode(m_iSchemePacketSize, p_ec_audio_packets, (unsigned int)m_uSchemeDataPackets, p_ec_audio_ecs, (unsigned int)m_uSchemeECPackets);
         
         for( u32 u=0; u<m_uSchemeECPackets; u++ )
         {
            uAudioPacketIndex = ((m_uCurrentTxAudioBlockIndex & 0xFFFFFF)<<8) | m_uCurrentTxAudioSegmentIndex;
            _sendAudioPacket(m_ListBufferedInputECPackets[u], m_iSchemePacketSize, uAudioPacketIndex);
            iCountPacketsSent++;

            m_uCurrentTxAudioSegmentIndex++;
         }
      }

      if ( m_uCurrentTxAudioSegmentIndex >= m_uSchemeDataPackets + m_uSchemeECPackets )
      {
         m_uCurrentTxAudioSegmentIndex = 0;
         m_uCurrentTxAudioBlockIndex++;
         if ( m_uCurrentTxAudioBlockIndex > 0xFFFFFF )
            m_uCurrentTxAudioBlockIndex = 0;
      }
   }
   return iCountPacketsSent;
}