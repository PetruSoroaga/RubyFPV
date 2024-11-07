#pragma once
#include "base.h"

class ParserH264
{
   public:
      ParserH264();
      virtual ~ParserH264();
      
      void init();

      // Returns number of start of a new frames that was found
      int parseData(u8* pData, int iDataLength, u32 uTimeNowMs);

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
      int m_iDetectedISlices;
      u32 m_uStreamCurrentParseToken;
      u32 m_uCurrentNALUType;
      u32 m_uLastNALUType;
      int m_iConsecutiveSlicesForCurrentNALU;
      
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

      // returns true if start of a new frame is detected (not of a new slice in a frame)
      bool _onParseStartOfNALUnit();
};
