/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
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
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "base.h"
#include "parser_h264.h"


ParserH264::ParserH264()
{
   init();
}

ParserH264::~ParserH264()
{
}

void ParserH264::init()
{
   m_szPrefix[0] = 0;
   m_iDetectedISlices = 1;
   m_iConsecutiveSlicesForCurrentNALU = 1;
   m_uTotalParsedBytes = 0;
   m_uLocalParsedBytes = 0;
   m_uStreamCurrentParsedToken = 0x11111111;
   m_uStreamPrevParsedToken = 0x11111111;
   m_uCurrentNALUType = 0;
   m_uLastNALUType = 0;
   m_uSizeCurrentFrame = 0;
   m_uSizeLastFrame = 0;
   m_iFramesSinceLastKeyframe = 0;
   m_iDetectedKeyframeIntervalInFrames = 1;

   m_uSegmentCurrentParsedToken = 0x11111111;
   m_uSegmentCurrentNALHeaderByte = 0;
   m_uSegmentCurrentNALType = 0;

   m_uTimeLastNALStart = 0;
   m_uTimeLastFPSCompute = 0;
   m_iFramesSinceLastFPSCompute = 0;
   m_iDetectedFPS = 0;

   m_iReadH264ProfileAfterBytes = -1;
   m_iReadH264ProfileConstrainsAfterBytes = -1;
   m_iReadH264LevelAfterBytes = -1;
   m_iDetectedH264Profile = 0;
   m_iDetectedH264ProfileConstrains = -1;
   m_iDetectedH264Level = 0;
}

void ParserH264::setPrefix(const char* szPrefix)
{
   if ( (NULL == szPrefix) || (0 == szPrefix[0]) )
   {
      m_szPrefix[0] = 0;
      return;
   }
   strncpy(m_szPrefix, szPrefix, sizeof(m_szPrefix)/sizeof(m_szPrefix[0]));
}


// A NAL is: [00 00 00 01] [NAL type] [data]
// NAL delimitator is used here at the start of the NAL, not at the end
// So it parses untill [00 00 00 01] is detected (but it's not included, will be included in the next parsing as the start of the NAL)

// Returns number of bytes parsed from input until start of NAL detected or end of segment
int ParserH264::parseSingleSegmentDataUntilStartOfNextNALOrLimit(u8* pData, int iDataLength, int iMaxBytes)
{
   m_uSegmentCurrentParsedToken = 0x11111111;
   if ( (NULL == pData) || (iDataLength <= 0) )
      return -1;

   int iCountParsed = 0;
   while ( iDataLength > 0 )
   {
      m_uSegmentCurrentParsedToken = (m_uSegmentCurrentParsedToken<<8) | (*pData);
      pData++;
      iDataLength--;
      iCountParsed++;
      if ( m_uSegmentCurrentParsedToken == 0x00000001 )
      {
          if ( (iCountParsed == 4) && (iDataLength > 0) )
          {
             m_uSegmentCurrentNALHeaderByte = *pData;
             m_uSegmentCurrentNALType = (*pData) & 0b11111;
          }
          if ( iCountParsed > 4 )
             return iCountParsed-4;
      }
      if ( iCountParsed >= iMaxBytes )
         break;
   }
   return iCountParsed;
}

// Returns the number of bytes parsed from input
int ParserH264::parseDataUntilStartOfNextNAL(u8* pData, int iDataLength, u32 uTimeNow)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return 0;

   m_uLocalParsedBytes = 0;
   int iBytesParsed = 0;
   while ( iDataLength > 0 )
   {
      m_uStreamPrevParsedToken = (m_uStreamPrevParsedToken << 8) | (m_uStreamCurrentParsedToken & 0xFF);
      m_uStreamCurrentParsedToken = (m_uStreamCurrentParsedToken<<8) | (*pData);
      m_uTotalParsedBytes++;
      m_uLocalParsedBytes++;
      iBytesParsed++;
      pData++;
      iDataLength--;
      m_uSizeCurrentFrame++;

      if ( m_iReadH264ProfileAfterBytes >= 0 )
      {
         m_iReadH264ProfileAfterBytes--;
         if ( 0 == m_iReadH264ProfileAfterBytes )
         {
            m_iDetectedH264Profile = m_uStreamCurrentParsedToken & 0xFF;
            log_line("Detected H264 stream profile: %d (0x%02X)", m_iDetectedH264Profile, (u8)m_iDetectedH264Profile);
         }
      }
      if ( m_iReadH264ProfileConstrainsAfterBytes >= 0 )
      {
         m_iReadH264ProfileConstrainsAfterBytes--;
         if ( 0 == m_iReadH264ProfileConstrainsAfterBytes )
         {
            m_iDetectedH264ProfileConstrains = m_uStreamCurrentParsedToken & 0xFF;
            log_line("Detected H264 stream profile constrains: %d (0x%02X)", m_iDetectedH264ProfileConstrains, (u8)m_iDetectedH264ProfileConstrains);
         }
      }
      if ( m_iReadH264LevelAfterBytes >= 0 )
      {
         m_iReadH264LevelAfterBytes--;
         if ( 0 == m_iReadH264LevelAfterBytes )
         {
            m_iDetectedH264Level = m_uStreamCurrentParsedToken & 0xFF;
            log_line("Detected H264 stream level: %d (0x%02X)", m_iDetectedH264Level, (u8)m_iDetectedH264Level);
         }
      }

      if ( m_uStreamPrevParsedToken == 0x00000001 )
      {
         _parseDetectedStartOfNALUnit(uTimeNow);
         return iBytesParsed;
      }
   }

   return iBytesParsed;
}

void ParserH264::_parseDetectedStartOfNALUnit(u32 uTimeNow)
{
   m_uLastNALUType = m_uCurrentNALUType;
   m_uCurrentNALUType = m_uStreamCurrentParsedToken & 0b11111;
   m_uSizeLastFrame = m_uSizeCurrentFrame;
   
   //log_line("DEBUG %s NAL %d, @pos %u, (consec: %d), lastNAL %d, lastNAL %d bytes, curr token 0x%08X, delta: %u ms",
   //   m_szPrefix, m_uCurrentNALUType, m_uLocalParsedBytes, m_iConsecutiveSlicesForCurrentNALU,
   //   m_uLastNALUType, m_uSizeLastFrame, m_uStreamCurrentParsedToken,
   //   uTimeNow - m_uTimeLastNALStart);
   
   m_uTimeLastNALStart = uTimeNow;
   m_uSizeCurrentFrame = 0;

   // Begin: compute slices based on Iframe
   if ( m_uCurrentNALUType == m_uLastNALUType )
      m_iConsecutiveSlicesForCurrentNALU++;
   else
   {
      if ( m_uLastNALUType == 5 )
      {
         m_iDetectedISlices = m_iConsecutiveSlicesForCurrentNALU;
         m_iDetectedKeyframeIntervalInFrames = m_iFramesSinceLastKeyframe/m_iDetectedISlices;
         m_iFramesSinceLastKeyframe = 0;
         //log_line("DEBUG detected slices: %d, detected keyframe interval in frames: %d", m_iDetectedISlices, m_iDetectedKeyframeIntervalInFrames);
      }
      m_iConsecutiveSlicesForCurrentNALU = 1;
   }

   // End: compute slices based on Iframe

   if ( (m_uCurrentNALUType == 5) || (m_uCurrentNALUType == 1) )
      m_iFramesSinceLastKeyframe++;

   if ( m_uCurrentNALUType == 7 )
   if ( (0 == m_iDetectedH264Level) || (0 == m_iDetectedH264Profile) || (-1 == m_iDetectedH264ProfileConstrains) )
   {
      m_iReadH264ProfileAfterBytes = 1;
      m_iReadH264ProfileConstrainsAfterBytes = 2;
      m_iReadH264LevelAfterBytes = 3;
   }

   m_iFramesSinceLastFPSCompute++;
   if ( 100 == m_iFramesSinceLastFPSCompute )
   {
      if ( uTimeNow != m_uTimeLastFPSCompute )
         m_iDetectedFPS = 100000/(uTimeNow - m_uTimeLastFPSCompute);
      if ( m_iDetectedISlices > 0 )
         m_iDetectedFPS /= m_iDetectedISlices;
      m_uTimeLastFPSCompute = uTimeNow;
      m_iFramesSinceLastFPSCompute = 0;
   }
}

bool ParserH264::IsInsideIFrame()
{
   return (m_uCurrentNALUType == 5)?true:false;
}

u32 ParserH264::getCurrentSegmentNALHeader()
{
   return m_uSegmentCurrentNALHeaderByte;
}
u32 ParserH264::getCurrentSegmentNALType()
{
   return m_uSegmentCurrentNALType;
}

u32 ParserH264::getCurrentFrameType()
{
   return m_uCurrentNALUType;
}

u32 ParserH264::getPreviousFrameType()
{
   return m_uLastNALUType;
}


u32 ParserH264::getSizeOfLastCompleteFrameInBytes()
{
   return m_uSizeLastFrame;
}

int ParserH264::getDetectedSlices()
{
   return m_iDetectedISlices;
}


int ParserH264::getDetectedFPS()
{
   return m_iDetectedFPS;
}

int ParserH264::getDetectedProfile()
{
   return m_iDetectedH264Profile;
}

int ParserH264::getDetectedProfileConstrains()
{
   return m_iDetectedH264ProfileConstrains;
}

int ParserH264::getDetectedLevel()
{
   return m_iDetectedH264Level;
}

void ParserH264::resetDetectedProfileAndLevel()
{
   m_iDetectedH264Profile = 0;
   m_iDetectedH264ProfileConstrains = -1;
   m_iDetectedH264Level = 0;
}

