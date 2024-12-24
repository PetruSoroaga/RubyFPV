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
   m_iConsecutiveSlicesForCurrentNALU = 0;
   m_uStreamCurrentParseToken = MAX_U32;
   m_uCurrentNALUType = 0;
   m_uLastNALUType = 0;
   m_uSizeCurrentFrame = 0;
   m_uSizeLastFrame = 0;
   m_iFramesSinceLastKeyframe = 0;
   m_iDetectedKeyframeIntervalInFrames = 1;

   m_uTimeLastFPSCompute = 0;
   m_iFramesSinceLastFPSCompute = 0;
   m_iDetectedFPS = 0;
}

// Returns the number of bytes parsed from input
int ParserH264::parseDataUntillStartOfNextNAL(u8* pData, int iDataLength, u32 uTimeNow)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return 0;

   int iBytesParsed = 0;
   while ( iDataLength > 0 )
   {
      m_uStreamCurrentParseToken = (m_uStreamCurrentParseToken<<8) | (*pData);
      // If not start a new NAL unit (00 00 01), continue parsing data
      if ( (m_uStreamCurrentParseToken & 0xFFFFFF00) != 0x0100 )
      {
         pData++;
         iDataLength--;
         iBytesParsed++;
         m_uSizeCurrentFrame++;
         continue;
      }
      _parseDetectedStartOfNALUnit(uTimeNow);
      break;
   }

   return iBytesParsed;
}

void ParserH264::_parseDetectedStartOfNALUnit(u32 uTimeNow)
{
   m_uLastNALUType = m_uCurrentNALUType;
   m_uCurrentNALUType = m_uStreamCurrentParseToken & 0b11111;
   m_uSizeLastFrame = m_uSizeCurrentFrame;
   //log_line("DEBUG start of NAL %d, last NAL %d, last NAL size: %d", m_uCurrentNALUType, m_uLastNALUType, m_uSizeLastFrame);
   m_uSizeCurrentFrame = 0;

   // Begin: compute slices based on Iframe
   if ( (m_uCurrentNALUType == 5) && (m_uLastNALUType != 5) )
      m_iConsecutiveSlicesForCurrentNALU = 1;
   if ( (m_uCurrentNALUType == 5) && (m_uLastNALUType == 5) )
      m_iConsecutiveSlicesForCurrentNALU++;
   if ( (m_uCurrentNALUType != 5) && (m_uLastNALUType == 5) )
   {
       m_iDetectedISlices = m_iConsecutiveSlicesForCurrentNALU;
       m_iConsecutiveSlicesForCurrentNALU = 0;
       m_iDetectedKeyframeIntervalInFrames = m_iFramesSinceLastKeyframe/m_iDetectedISlices;
       m_iFramesSinceLastKeyframe = 0;
   }

   // End: compute slices based on Iframe

   if ( (m_uCurrentNALUType == 5) || (m_uCurrentNALUType == 1) )
      m_iFramesSinceLastKeyframe++;
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