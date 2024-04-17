#pragma once
#include "base.h"

class ParserH264
{
   public:
      ParserH264();
      virtual ~ParserH264();
      
      void init(int iExpectedISlices);

      // Returns true if an start of a new frame was found
      bool parseData(u8* pData, int iDataLength, u32 uTimeNowMs);

      u32 getStartTimeOfCurrentFrame();
      u32 getCurrentFrameType();
      u32 getStartTimeOfLastIFrame();
      u32 getSizeOfLastCompleteFrame();
      u32 getTimeDurationOfLastCompleteFrame();
      int getDetectedSlices();
      u32 getCurrentlyDetectedKeyframeIntervalMs();
      bool IsInsideIFrame();
      u32 getFramesSinceLastKeyframe();
      u32 getDetectedFPS();

   protected:
      int m_iExpectedISlices;
      int m_iDetectedISlices;
      int m_iStateCurrentParsedSlices;
      u32 m_uStateCurrentToken;
      bool m_bStateIsInsideIFrame;
      u32 m_uCurrentNALUType;
      u32 m_uLastNALUType;
      u32 m_uConsecutiveNALUs;
      u32 m_uCurrentFrameType;
      u32 m_uLastFrameType;
      u32 m_uTimeStartOfCurrentFrame;
      u32 m_uTimeLastStartOfIFrame;
      u32 m_uTimeDurationOfLastFrame;
      u32 m_uSizeCurrentFrame;
      u32 m_uSizeLastFrame;
      u32 m_uCurrentDetectedKeyframeIntervalMs;
      u32 m_uFramesSinceLastKeyframe;

      u32 m_uDebugFramesCounter;
      u32 m_uDebugTimeStartFramesCounter;
      u32 m_uDebugDetectedFPS;
};
