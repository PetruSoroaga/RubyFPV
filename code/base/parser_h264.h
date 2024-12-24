#pragma once
#include "base.h"

class ParserH264
{
   public:
      ParserH264();
      virtual ~ParserH264();
      
      void init();

      // Returns number of bytes parsed from input untill start of NAL detected
      int parseDataUntillStartOfNextNAL(u8* pData, int iDataLength, u32 uTimeNow);

      bool IsInsideIFrame();
      u32 getCurrentFrameType();
      u32 getPreviousFrameType();
      u32 getSizeOfLastCompleteFrameInBytes();
      int getDetectedSlices();
      int getDetectedFPS();
      
   protected:
      u32 m_uStreamCurrentParseToken;
      u32 m_uCurrentNALUType;
      u32 m_uLastNALUType;
      int m_iDetectedISlices;
      int m_iConsecutiveSlicesForCurrentNALU;
      
      u32 m_uSizeCurrentFrame;
      u32 m_uSizeLastFrame;
      int m_iDetectedKeyframeIntervalInFrames;
      int m_iFramesSinceLastKeyframe;

      u32 m_uTimeLastFPSCompute;
      int m_iFramesSinceLastFPSCompute;
      int m_iDetectedFPS;
      void _parseDetectedStartOfNALUnit(u32 uTimeNow);
};
