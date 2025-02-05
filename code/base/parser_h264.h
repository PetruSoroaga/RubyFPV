#pragma once
#include "base.h"

class ParserH264
{
   public:
      ParserH264();
      virtual ~ParserH264();
      
      void init();
      void setPrefix(const char* szPrefix);

      // Returns number of bytes parsed from input until start of NAL detected
      int parseDataUntilStartOfNextNALOrLimit(u8* pData, int iDataLength, int iMaxToParse, u32 uTimeNow);
      bool lastParseDetectedNALStart();
      bool IsInsideIFrame();
      u32 getCurrentNALType();
      u32 getPreviousNALType();
      u32 getSizeOfLastCompleteFrameInBytes();
      int getDetectedSlices();
      int getCurrentFrameSlices();
      int getDetectedFPS();
      int getDetectedProfile();
      int getDetectedProfileConstrains();
      int getDetectedLevel();
      void resetDetectedProfileAndLevel();
      
   protected:
      void _parseDetectedStartOfNALUnit(u32 uTimeNow);

      char m_szPrefix[64];
      u32 m_uTotalParsedBytes;
      u32 m_uStreamCurrentParsedToken;
      u32 m_uStreamPrevParsedToken;
      u32 m_uCurrentNALUType;
      u32 m_uLastNALUType;
      int m_iDetectedISlices;
      int m_iConsecutiveSlicesForCurrentNALU;
      
      u32 m_uSizeCurrentFrame;
      u32 m_uSizeLastFrame;
      int m_iDetectedKeyframeIntervalInFrames;
      int m_iFramesSinceLastKeyframe;

      u32 m_uTimeLastNALStart;
      u32 m_uTimeLastFPSCompute;
      int m_iFramesSinceLastFPSCompute;
      int m_iDetectedFPS;

      bool m_bLastParseDetectedNALStart;
      int m_iReadH264ProfileAfterBytes;
      int m_iReadH264ProfileConstrainsAfterBytes;
      int m_iReadH264LevelAfterBytes;
      int m_iDetectedH264Profile;
      int m_iDetectedH264ProfileConstrains;
      int m_iDetectedH264Level;
};
