#pragma once

#include "../base/hardware.h"
#include "../base/ctrl_preferences.h"
#define CONTROLLER_SETTINGS_STAMP_ID "vV.6"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
   int iUseBrokenVideoCRC;
   int iTXPower;
   int iTXPowerAtheros;
   int iTXPowerRTL;
   int iTXPowerSiK;
   int iMaxTXPower;
   int iMaxTXPowerAtheros;
   int iMaxTXPowerRTL;
   int iHDMIBoost;
   int iOverVoltage; // 0 - disabled
   int iFreqARM; // 0 - disabled
   int iFreqGPU; // 0 - disabled
   int iNiceRouter; // 0 - disabled
   int ioNiceRouter; // 0 or negative - disabled
   int iNiceCentral; // 0 - disabled
   int iNiceRXVideo; // 0 - auto
   int ioNiceRXVideo; // 0 or negative - disabled
   int iVideoForwardUSBType; // 0 - none, 1 - raw (h264)
   int iVideoForwardUSBPort;
   int iVideoForwardUSBPacketSize;
   int nVideoForwardETHType; // 0 - none, 1 - raw (h264), 2 - rtp (gstreamer)
   int nVideoForwardETHPort;
   int nVideoForwardETHPacketSize;
   int iTelemetryForwardUSBType; // 0 - none, 1 - mavlink
   int iTelemetryForwardUSBPort;
   int iTelemetryForwardUSBPacketSize;
   int iTelemetryOutputSerialPortIndex;  // -1 for disabled
   int iTelemetryInputSerialPortIndex;   // -1 for disabled
   int iMAVLinkSysIdController;
   int iDisableHDMIOverscan;
   int iDeveloperMode;
   int iRenderFPS;
   int iShowVoltage;
   int nRetryRetransmissionAfterTimeoutMS;
   int nRequestRetransmissionsOnVideoSilenceMs;
   int nUseFixedIP;
   u32 uFixedIP;
   int nAutomaticTxCard;
   int nRotaryEncoderFunction;
   int nRotaryEncoderSpeed;
   int nRotaryEncoderFunction2;
   int nRotaryEncoderSpeed2;
   int nPingClockSyncFrequency;
   int nGraphRadioRefreshInterval;
   int nGraphVideoRefreshInterval;
   int iDisableRetransmissionsAfterControllerLinkLostMiliseconds; // 0 to disable functionality
   int iVideoDecodeStatsSnapshotClosesOnTimeout;
   int iFreezeOSD;
   int iDevSwitchVideoProfileUsingQAButton;
   int iShowControllerAdaptiveInfoStats;
   int iShowVideoStreamInfoCompactType;

   int iSearchSiKAirRate;
   int iSearchSiKECC;
   int iSearchSiKLBT;
   int iSearchSiKMCSTR;

   int iAudioOutputDevice;
   int iAudioOutputVolume;

   int iDevRxLoopTimeout;
   u32 uShowBigRxHistoryInterface;

   int iSiKPacketSize;

   int iRadioRxThreadPriority;
   int iRadioTxThreadPriority;
} ControllerSettings;

int save_ControllerSettings();
int load_ControllerSettings();
void reset_ControllerSettings();
ControllerSettings* get_ControllerSettings();

u32 compute_ping_interval_ms(u32 uModelFlags, u32 uRxTxSyncType, u32 uCurrentVideoProfileFlags);

#ifdef __cplusplus
}  
#endif 