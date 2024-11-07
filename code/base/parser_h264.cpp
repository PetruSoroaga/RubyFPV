/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga
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
   m_iDetectedISlices = 1;
   m_uStreamCurrentParseToken = MAX_U32;
   m_uCurrentNALUType = 0;
   m_uLastNALUType = 0;
   m_iConsecutiveSlicesForCurrentNALU = 0;
   m_uTimeDurationOfLastFrame = 0;
   m_uTimeStartOfCurrentFrame = 0;
   m_uTimeLastStartOfIFrame = 0;
   m_uSizeCurrentFrame = 0;
   m_uSizeLastFrame = 0;
   m_uCurrentDetectedKeyframeIntervalMs = 0;
   m_uFramesSinceLastKeyframe = 0;

   m_uDebugFramesCounter = 0;
   m_uDebugTimeStartFramesCounter = 0;
   m_uDebugDetectedFPS = 0;
}

// Returns the number of starts of a new frames if it was found
int ParserH264::parseData(u8* pData, int iDataLength, u32 uTimeNowMs)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return 0;

   int iFoundFrameStartCount = 0;

   while ( iDataLength > 0 )
   {
      m_uStreamCurrentParseToken = (m_uStreamCurrentParseToken<<8) | (*pData);
      pData++;
      iDataLength--;
      m_uSizeCurrentFrame++;

      // If not start a new NAL unit (00 00 01), continue parsing data
      if ( (m_uStreamCurrentParseToken & 0xFFFFFF00) != 0x0100 )
         continue;

      if ( _onParseStartOfNALUnit() )
      {
         iFoundFrameStartCount++;
         m_uDebugFramesCounter++;
         m_uTimeDurationOfLastFrame = uTimeNowMs - m_uTimeStartOfCurrentFrame;
         m_uTimeStartOfCurrentFrame = uTimeNowMs;

         if ( m_uCurrentNALUType == 5 )
         {
            m_uCurrentDetectedKeyframeIntervalMs = uTimeNowMs - m_uTimeLastStartOfIFrame;
            m_uTimeLastStartOfIFrame = uTimeNowMs;
            m_uFramesSinceLastKeyframe = 0;
         }

         if ( uTimeNowMs >= m_uDebugTimeStartFramesCounter + 1000 )
         {
            m_uDebugTimeStartFramesCounter = uTimeNowMs;
            m_uDebugDetectedFPS = m_uDebugFramesCounter;
            m_uDebugFramesCounter = 0;
         }
      }
   }

   return iFoundFrameStartCount;
}

// returns true if start of a new frame is detected (not of a new slice in a frame)
bool ParserH264::_onParseStartOfNALUnit()
{
   m_uCurrentNALUType = m_uStreamCurrentParseToken & 0b11111;

   // We started a new frame P,I or SPP PSP frames ?
   bool bStartedNewFrame = false;

   if ( m_uCurrentNALUType != m_uLastNALUType )
   {
      bStartedNewFrame = true;
      if ( m_uLastNALUType == 5 )
         m_iDetectedISlices = m_iConsecutiveSlicesForCurrentNALU;
         
      m_iConsecutiveSlicesForCurrentNALU = 0;
      m_uLastNALUType = m_uCurrentNALUType;
   }
   else if ( (m_iConsecutiveSlicesForCurrentNALU >= m_iDetectedISlices) && (m_uCurrentNALUType == 1) )
   {
      bStartedNewFrame = true;
      m_iConsecutiveSlicesForCurrentNALU = 0;
   }
   m_iConsecutiveSlicesForCurrentNALU++;

   // P-frame is 1, I-frame is 5
   if ( (m_uCurrentNALUType != 1) && (m_uCurrentNALUType != 5) )
      return bStartedNewFrame;

   // P or I frame (slice) just started. Compute info

   if ( bStartedNewFrame )
   {
      m_uFramesSinceLastKeyframe++;
      m_uSizeLastFrame = m_uSizeCurrentFrame;
      m_uSizeCurrentFrame = 0;
   }
   return bStartedNewFrame;
}

u32 ParserH264::getStartTimeOfCurrentFrame()
{
   return m_uTimeStartOfCurrentFrame;
}

u32 ParserH264::getCurrentFrameType()
{
   return m_uCurrentNALUType;
}

u32 ParserH264::getStartTimeOfLastIFrame()
{
   return m_uTimeLastStartOfIFrame;
}

u32 ParserH264::getSizeOfLastCompleteFrame()
{
   return m_uSizeLastFrame;
}

u32 ParserH264::getTimeDurationOfLastCompleteFrame()
{
   return m_uTimeDurationOfLastFrame;
}

int ParserH264::getDetectedSlices()
{
   return m_iDetectedISlices;
}

u32 ParserH264::getCurrentlyDetectedKeyframeIntervalMs()
{
   return m_uCurrentDetectedKeyframeIntervalMs;
}

bool ParserH264::IsInsideIFrame()
{
   if ( m_uCurrentNALUType == 5 )
      return true;
   return false;
}

u32 ParserH264::getFramesSinceLastKeyframe()
{
   return m_uFramesSinceLastKeyframe;
}

u32 ParserH264::getDetectedFPS()
{
   return m_uDebugDetectedFPS;
}
