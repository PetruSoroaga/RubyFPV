#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/shared_mem_controller_only.h"
#include "processor_rx_video.h"

class ProcessorRxVideoWFBOHD: public ProcessorRxVideo
{
   public:
      ProcessorRxVideoWFBOHD(u32 uVehicleId, u32 uVideoStreamIndex);
      virtual ~ProcessorRxVideoWFBOHD();

      virtual bool init();
      virtual bool uninit();
      virtual void resetState();
      
      virtual void periodicLoop(u32 uTimeNow);
      virtual int handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length);

   protected:
      
};

