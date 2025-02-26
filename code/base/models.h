#pragma once
#include "base.h"
#include "hardware.h"
#include "flags.h"
#include "config_rc.h"
#include "shared_mem.h"
#include "../radio/radiopackets2.h"

#define MODEL_TELEMETRY_TYPE_NONE 0
#define MODEL_TELEMETRY_TYPE_MAVLINK 1
#define MODEL_TELEMETRY_TYPE_LTM 2

#define MODEL_MAX_CAMERAS 4
#define MODEL_CAMERA_PROFILES 3

#define MODEL_MAX_OSD_PROFILES 5

#define CAMERA_FLAG_FORCE_MODE_1 1
#define CAMERA_FLAG_IR_FILTER_OFF ((u32)(((u32)0x01)<<2))
#define CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF ((u32)(((u32)0x01) << 3))
#define CAMERA_FLAG_OPENIPC_3A_SIGMASTAR ((u32)(((u32)0x01) << 4))

typedef struct
{
   u32 uFlags; // See CAMERA_FLAG_... above
   // bit 0: force mode 1
   // bit 1: AWB mode old
   // bit 2: 1: turn IR Cut off
   // bit 3: 1: turn off daylight mode for OpenIPC cameras, activate night mode
   // bit 4: 1: use sigmastar 3a, 0: use default 3a
   u8 flip_image;
   u8 brightness; // 0...100
   u8 contrast;   // 0...100
   u8 saturation; // 0...200,  100 is middle (0)
   u8 sharpness;
   u8 exposure;
   u8 whitebalance;
   u8 metering;
   u8 drc; // stands for AGC (gain) for IMX327 camera (0 to 0xF, 0xC default)
   float analogGain;
   float awbGainB;
   float awbGainR; // stands for HUE for IMX307 camera
   float fovV;
   float fovH;
   u8 vstab; // on/off
   u8 ev;    // -10 to 10, 0 default, translated to 1...21, default 11
   u16 iso; // 100 - 800, 0 for off
   u16 shutterspeed; // in 1/x of a second, 0 for off, min is 30, max is 30000 (1/30 to 1/30000);
      // For IMX415 OpenIPC is exposure in miliseconds
   u8 wdr; // used for IMX327 camera for WDR mode
   u8 dayNightMode; // 0 - day mode, 1 - night mode, only for Veye cameras
   u8 hue; // 0...100
   u8 dummyCamP[1];
} camera_profile_parameters_t;

typedef struct
{
   camera_profile_parameters_t profiles[MODEL_CAMERA_PROFILES];
   int iCurrentProfile;
   int iCameraType; // as detected by hardware. 0 for none
   int iForcedCameraType; // as set by user. 0 for none
   char szCameraName[MAX_CAMERA_NAME_LENGTH];

} type_camera_parameters;


typedef struct
{
   int user_selected_video_link_profile; // set by user on the controller
   int iH264Slices;
   int iRemovePPSVideoFrames;
   int iInsertPPSVideoFrames;
   int iInsertSPTVideoFramesTimings;
   int videoAdjustmentStrength; // 1..10 (from 10% to 100% strength)
   u32 lowestAllowedAdaptiveVideoBitrate;
   u32 uMaxAutoKeyframeIntervalMs; // in milisec
   u32 uVideoExtraFlags; // Check VIDEO_FLAG_* enum
    // bit 0: Fill H264 SPS timings 
    // bit 1: Ignore Tx spikes
    // bit 2: enable HDMI output
    // bit 3: retransmissions are started fast
    // bit 4: 1 to enable H265, 0 to enable H264
    // bit 5: 1 to enable new adaptive video algorithm, 0 - use default one

   u32 dummy[3];
} video_parameters_t;


typedef struct
{
   u32 uProfileFlags;
      // VIDEO_PROFILE_FLAGS_* constants
      // bit 0-1: 3d noise: 0,1, auto (2)

   u32 uProfileEncodingFlags; // same as radio video packet uProfileEncodingFlags
   // VIDEO_PROFILE_ENCODING_FLAG_* constants
   // byte 0:
   //    bit 0..2  - scramble blocks count
   //    bit 3     - enables restransmission of missing packets
   //    bit 4     - enable adaptive video keyframe interval
   //    bit 5     - enable adaptive video link params
   //    bit 6     - use controller info too when adjusting video link params
   //    bit 7     - go lower adaptive video profile when controller link lost

   // byte 1:   - max time to wait for retransmissions (in ms*5)// affects rx buffers size
   // byte 2:   - retransmission duplication percent (0-100%), 0xFF = auto, bit 0..3 - regular packets duplication, bit 4..7 - retransmitted packets duplication
   // byte 3:
   //    bit 0  - use medium adaptive video
   //    bit 1  - enable video auto quantization
   //    bit 2  - video auto quantization strength
   //    bit 3  - one way video link
   //    bit 4  - video profile should use EC scheme as auto;
   //    bit 5,6 - EC scheme spreading factor (0...3)
   //    bit 7  - try to keep constant video bitrate when it fluctuates

   int radio_datarate_video_bps; // radio data rate to use for this video profile for video packets: 0 - to use auto datarate, positive: bps, negative: MCS
   int radio_datarate_data_bps;  // radio data rate to use for this video profile for data packets: 0 - to use auto datarate, positive: bps, negative: MCS
   
   u32 radio_flags; // 0 if no custom ones, use the radio link radio flags
   int h264profile; // 0 = baseline, 1 = main, 2 = high
   int h264level; //0 = 4.0, 1 = 4.1, 2 = 4.2
   int h264refresh; // 0 = cyclic, 1 = adaptive, 2 = both, 3 = cyclicrows
   int h264quantization; // 0 - auto, // pozitive - value to use, // negative - value when disabled (auto)
   int iIPQuantizationDelta;

   int width;
   int height;
   int block_packets;
   int block_fecs;
   int video_data_length;
   int keyframe_ms;
   // v 7.7: changed keyframe to miliseconds instead of frames count
   int fps;
   u32 bitrate_fixed_bps; // in bits per second, 0 for auto

} type_video_link_profile;


typedef struct
{
   int iCurrentOSDLayout;
   bool voltage_alarm_enabled;
   float voltage_alarm;
   int  battery_show_per_cell;
   int  battery_cell_count; // 0 - autodetect
   int  battery_capacity_percent_alarm; // 0 for disabled
   bool altitude_relative;
   bool show_overload_alarm;
   bool show_stats_rx_detailed;
   bool show_stats_decode;
   bool show_stats_rc;
   bool show_full_stats;
   bool invert_home_arrow;
   int  home_arrow_rotate; // degrees
   bool show_instruments;
   int ahi_warning_angle;
   bool show_gps_position;
   u32 osd_flags[MODEL_MAX_OSD_PROFILES]; // what OSD elements to display for each layout mode
   u32 osd_flags2[MODEL_MAX_OSD_PROFILES];
   u32 osd_flags3[MODEL_MAX_OSD_PROFILES];
   u32 instruments_flags[MODEL_MAX_OSD_PROFILES]; // each bit: what instrument to show
   u32 osd_preferences[MODEL_MAX_OSD_PROFILES];
     // byte 0: osd elements font size (0...6)
     // byte 1: transparency (0: max ... 4: none)
     // byte 2: bit 0...3  osd stats windows font size (0...6)
     //         bit 4...7  osd stats background transparency (0...3)
     // byte 3:
     //    bit 0: show controller link lost alarm
     //    bit 1: arrange osd stats windows top
     //    bit 2: arrange osd stats windows bottom
     //    bit 3: arrange osd stats windows left
     //    bit 4: arrange osd stats windows right
     //    bit 5: do not show messages (texts) from FC in OSD
     //

   u32 uFlags;
     // bit 0,1,2: MSO OSD font: 0 - auto, 1 - BF, 2 - INAV
     // bit 3: show flight end stats
     // bit 4: show temps in F
} osd_parameters_t;


typedef struct
{
   int isRelayEnabledOnRadioLinkId; // negative: disabled, positive: radioLinkId 
   u32 uRelayFrequencyKhz;
   u32 uRelayedVehicleId;
   u32 uRelayCapabilitiesFlags;
   u8  uCurrentRelayMode;
} type_relay_parameters;

#define RC_TRANSLATION_TYPE_NONE 0
#define RC_TRANSLATION_TYPE_2000 1
#define RC_TRANSLATION_TYPE_4000 2

typedef struct
{
   bool rc_enabled;
   int rc_frames_per_second;
   int rc_failsafe_timeout_ms;
   bool dummy1;
   int receiver_type;
   int inputType; // RC input type on the controller: 0 none, 1 usb, 2 ibus/sbus, see config_rc.h enum
   int inputSerialPort;
   long inputSerialPortSpeed;
   int outputSerialPort;
   long outputSerialPortSpeed;

   u32 rcChAssignment[MAX_RC_CHANNELS];
      // first byte:
          // bit 0: no assignment (0/1);
          // bit 1: 0 - axe, 1 - button;
          // bit 2: 0 - momentary, 1 - sticky button (togle);
          // bit 3: not used
          // bit 4..7 axe/button index (1 index based: first button/axe is 1)
          // each additional 4 bits: positive value (!=0) for each extra button (in increasing order of output RC value)
          // bit 31: toggle flag

   u16 rcChMid[MAX_RC_CHANNELS];
   u16 rcChMin[MAX_RC_CHANNELS];
   u16 rcChMax[MAX_RC_CHANNELS];
   u16 rcChFailSafe[MAX_RC_CHANNELS];
   u8 rcChExpo[MAX_RC_CHANNELS];  // expo (%)
   u8 rcChFlags[MAX_RC_CHANNELS]; 
          // bit 0: reversed
          // bit 1..3: failsafe type for this channel
          // bit 4: use linear range
          // bit 5: relative move

   u32 failsafeFlags; // first byte: what type of failsafe to execute, globally;
                      // 2nd-3rd byte: failsafe value (for that type of failsafe)
   int channelsCount;
   u32 hid_id; // USB HID id on the controller for RC control
   u32 flags;
          // bit 0: output to FC enabled
   u32 rcChAssignmentThrotleReverse;
   int iRCTranslationType;
   u32 rcDummy[8];
} rc_parameters_t;


#define TELEMETRY_TYPE_NONE 0
#define TELEMETRY_TYPE_MAVLINK 1
#define TELEMETRY_TYPE_LTM 2
#define TELEMETRY_TYPE_MSP 3
#define TELEMETRY_TYPE_TRANSPARENT 10

typedef struct
{
   int fc_telemetry_type; // 0 = None, 1 = MAVLink, 2 == LTM, 3 == MSP

   int iVideoBitrateHistoryGraphSampleInterval;
   u32 dummy2;

   int dummy5;
   u32 dummy6;

   bool bControllerHasInputTelemetry; // gets updated by the controller, in a get_all_params message. it's global
   bool bControllerHasOutputTelemetry; // gets updated by the controller, in a get_all_params message. it's global
   int controller_telemetry_type;
   int dummy3;
   u32 dummy4;

   int update_rate; // times per second
   int vehicle_mavlink_id;
   int controller_mavlink_id;
   u32 flags; // see flags.h for TELEMETRY_FLAGS_[ID] values
              // rx only, request data streams, spectator, send full mavlink/ltm packets to controller etc
              // usb uarts present (0,1,2)

   int dummy;
} telemetry_parameters_t;

typedef struct
{
   bool has_audio_device;
   bool enabled;
   int  volume;
   int  quality; // 0-3
   u8  uECScheme; // high 4 bits data, low 4 bits ec
   u16 uPacketLength;
   u32 uFlags;
      // byte 0:
      //   bit 0,1: mic type: 0 - none, 1 - internal, 2 - external
      // byte 1:
      //   0...255 buffering size
   u32 uDummyA1;
} audio_parameters_t;


#define MAX_MODEL_I2C_BUSSES 6
#define MAX_MODEL_I2C_DEVICES 16
#define MAX_MODEL_SERIAL_PORTS 6
#define MAX_MODEL_SERIAL_PORT_NAME 16

#define MODEL_SERIAL_PORT_BIT_EXTRNAL_USB ((u32)(((u32)0x01)<<11))
#define MODEL_SERIAL_PORT_BIT_SUPPORTED ((u32)(((u32)0x01)<<12))
typedef struct
{
   int i2c_bus_count;
   int i2c_device_count;
   int i2c_bus_numbers[MAX_MODEL_I2C_BUSSES];
   int i2c_devices_address[MAX_MODEL_I2C_DEVICES];
   int i2c_devices_bus[MAX_MODEL_I2C_DEVICES];
   int radio_interface_count;

   int serial_port_count;
   char serial_port_names[MAX_MODEL_SERIAL_PORTS][MAX_MODEL_SERIAL_PORT_NAME];
   u32 serial_port_supported_and_usage[MAX_MODEL_SERIAL_PORTS];
     // byte 0: usage type; same as hardware_serial enum: SERIAL_PORT_USAGE_xxxx
     // byte 1: bits 0...3 usb or hardware port index
     //         bits 4 : 0 - hardware builtin, 1 - on usb
     //         bits 5 : supported 0/1

   int serial_port_speed[MAX_MODEL_SERIAL_PORTS];
} type_vehicle_hardware_interfaces_info;


typedef struct
{
   u32 uCurrentOnTime; // seconds
   u32 uCurrentFlightTime; // seconds
   u32 uCurrentFlightDistance; // in 1/100 meters (cm)
   u32 uCurrentFlightTotalCurrent; // 0.1 miliAmps (1/10000 amps);

   u32 uCurrentTotalCurrent; // 0.1 miliAmps (1/10000 amps);
   u32 uCurrentMaxAltitude; // meters
   u32 uCurrentMaxDistance; // meters
   u32 uCurrentMaxCurrent; // miliAmps (1/1000 amps)
   u32 uCurrentMinVoltage; // miliVolts (1/1000 volts)

   u32 uTotalFlights; // count

   u32 uTotalOnTime;  // seconds
   u32 uTotalFlightTime; // seconds
   u32 uTotalFlightDistance; // in 1/100 meters (cm)

   u32 uTotalTotalCurrent; // 0.1 miliAmps (1/10000 amps);
   u32 uTotalMaxAltitude; // meters
   u32 uTotalMaxDistance; // meters
   u32 uTotalMaxCurrent; // miliAmps (1/1000 amps)
   u32 uTotalMinVoltage; // miliVolts (1/1000 volts)
} type_vehicle_stats_info;



typedef struct
{
   bool bEnableRCTriggerFreqSwitchLink1;
   bool bEnableRCTriggerFreqSwitchLink2;
   bool bEnableRCTriggerFreqSwitchLink3;

   int iRCTriggerChannelFreqSwitchLink1;
   int iRCTriggerChannelFreqSwitchLink2;
   int iRCTriggerChannelFreqSwitchLink3;

   bool bRCTriggerFreqSwitchLink1_is3Position;
   bool bRCTriggerFreqSwitchLink2_is3Position;
   bool bRCTriggerFreqSwitchLink3_is3Position;

   u32 uChannels433FreqSwitch[3];
   u32 uChannels868FreqSwitch[3];
   u32 uChannels23FreqSwitch[3];
   u32 uChannels24FreqSwitch[3];
   u32 uChannels25FreqSwitch[3];
   u32 uChannels58FreqSwitch[3];

   u32 dummy[12];

} type_functions_parameters;

// Radio datarates:
// zero: auto or use radio link datarate
// negative: MCS data rates
// positive < 100: legacy 2.4/5.8 datarates, in Mbps
// positive > 100: bps bitrates (for low bandwidth radio cards). Ex: 200 = 100 bps; 9700 = 9600 bps, 1000000 = 999900 bps

typedef struct
{
   //int txPowerRTL8812AU;
   //int txPowerRTL8812EU;
   //int txPowerAtheros;
   //int txPowerSiK;
   //int txMaxPowerRTL8812AU;
   //int txMaxPowerRTL8812EU;
   //int txMaxPowerAtheros;
   //int slotTime;
   //int thresh62;
   int iAutoVehicleTxPower;
   int iAutoControllerTxPower;
   int iDummyR3;
   int iDummyR4;
   int iDummyR5;
   int iDummyR6;
   int iDummyR7;
   int iDummyR8;
   int iDummyR9;

   int  interfaces_count;
   int  interface_card_model[MAX_RADIO_INTERFACES]; // 0 or positive - autodetected, negative - user set
   int  interface_link_id[MAX_RADIO_INTERFACES];
   int  interface_raw_power[MAX_RADIO_INTERFACES];
   u32  interface_radiotype_and_driver[MAX_RADIO_INTERFACES]; // low byte: radio type, second byte = driver type, third byte: supported card flag
   u8   interface_supported_bands[MAX_RADIO_INTERFACES];  // bits 0-3: 2.3, 2.4, 2.5, 5.8 bands
   char interface_szMAC[MAX_RADIO_INTERFACES][MAX_MAC_LENGTH];
   char interface_szPort[MAX_RADIO_INTERFACES][6]; // first byte - first char, sec byte - sec char
   u32  interface_capabilities_flags[MAX_RADIO_INTERFACES]; // what the card is used for: video/data/relay/tx/rx
   u32  interface_current_frequency_khz[MAX_RADIO_INTERFACES]; // current frequency for this card
   u32  interface_current_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: legacy/MCS datarate type, frame type, STBC, LDP, MCS etc
   
   int  interface_dummy2[MAX_RADIO_INTERFACES];

} type_radio_interfaces_parameters;


#define FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO 0
#define FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED 1
#define FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST 2
#define FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO 3

typedef struct
{
   int links_count;
   u32 link_frequency_khz[MAX_RADIO_INTERFACES];
   u32 link_capabilities_flags[MAX_RADIO_INTERFACES]; // data/video/both?
   u32 link_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: legacy/MCS datarate type, frame type, STBC, LDP, MCS, SIK flags, etc
   int link_datarate_video_bps[MAX_RADIO_INTERFACES]; // positive: bps, negative (-1 or less): MCS rate
   int link_datarate_data_bps[MAX_RADIO_INTERFACES]; // positive: bps, negative (-1 or less): MCS rate

   u8  uSerialPacketSize[MAX_RADIO_INTERFACES]; // packet size over air for serial radio links
   u32 uDummy2[MAX_RADIO_INTERFACES];
   int uplink_datarate_video_bps[MAX_RADIO_INTERFACES]; // positive: bps, negative (-1 or less): MCS rate
   int uplink_datarate_data_bps[MAX_RADIO_INTERFACES]; // positive: bps, negative (-1 or less): MCS rate

   u8  uUplinkDataDataRateType[MAX_RADIO_INTERFACES]; // DataRate Type
   u8  uDownlinkDataDataRateType[MAX_RADIO_INTERFACES];
   int iSiKPacketSize;
   u32 uGlobalRadioLinksFlags; // See MODEL_RADIOLINKS_FLAGS_... in base/flags.h
   // bit 0 - none, 1 - disable uplinks
   // bit 1 - bypass sockets buffers
   // bit 2 - has negotiated links;
   
   u32 uDummyRadio[7];
} type_radio_links_parameters;

typedef struct
{
   u8 uAlarmMotorCurrentThreshold; // in 0.1 amp increments, most significant bit: 0 - alarm disabled, 1 - alarm enabled

} type_alarms_parameters;


typedef struct
{
   u8 uLogTypeFlags;
   // 0 - no log
   // 1 - log when armed
   // 2 - log when recording
   u32 uLogIntervalMilisec;
   u32 uLogParams; // bitmask
   u32 uLogParams2; // bitmask
   u32 uDummy;
} type_logging_parameters;

#define PROCESSES_FLAGS_BALANCE_INT_CORES ((u32)(((u32)0x01)<<1))

typedef struct 
{
   int iNiceRC;
   int iNiceRouter; // negative value; 0 for disabled/auto
   int ioNiceRouter;
   int iNiceTelemetry;
   int iNiceVideo; // negative value; 0 - disabled/auto
   int ioNiceVideo; // 0 or negative - disabled;
   int iNiceOthers;
   int iOverVoltage; // 0 or negative - disabled, negative - default value
   int iFreqARM; // 0 or negative - disabled; in Mhz
   int iFreqGPU;
      // Pi: 0 or negative - disabled; in Mhz
      // OIPC: 0 or negative - default; positive: boosted
   int iThreadPriorityRouter; // 0 - disabled, 1...99, higher number - higher priority
   int iThreadPriorityRadioRx; // 0 - disabled, 1...99, higher number - higher priority
   int iThreadPriorityRadioTx; // 0 - disabled, 1...99, higher number - higher priority
   u32 uProcessesFlags;
} type_processes_priorities;


// This is all readonly:
typedef struct 
{
   u32 uBoardType; //byte 0: board_type;  byte 1: variant
   int iMaxTxVideoBlocksBuffer; // max blocks that can be cached on vehicle
   int iMaxTxVideoBlockPackets; // max packets in a video block
   u32 uFlags;
   u32 uRubyBaseVersion;
   int dummyhwc[1];
   u32 dummyhwc2[3];
} type_hardware_capabilities;

class Model
{
   public:
      Model();

      bool b_mustSyncFromVehicle; // non persistent
      u32 uDummyM1;
      u32 uDeveloperFlags;
        // byte 0:
        //    bit 0: enable live log
        //    bit 1: enable radio silence failsafe (reboot)
        //    bit 2: log only errors
        //    bit 3: enable vehicle developer video link stats
        //    bit 4: enable vehicle developer video link graphs
        //    bit 5: inject video faults (for testing only)
        //    bit 6: disable video tx overload logic
        //    bit 7: send back vehicle tx gap
        //     
        // byte 1:
        //    wifi guard delay (1..100 milisec)

        // byte 3:
        //    bit 0: send to controller video bitrate hystory telemetry packet
        //    bit 1: inject recoverable video faults

      u32 uModelFlags;
        //   bit 0: prioritize uplink
        //   bit 1: use logger service instead of files

      type_hardware_capabilities hwCapabilities;
      
      char vehicle_name[MAX_VEHICLE_NAME_LENGTH];
      u32 uVehicleId;
      u32 uControllerId;
      u32 sw_version; // byte 0: minor version, byte 1 Major version, byte 2-3: build nb
      bool is_spectator;
      u8 vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
      int rxtx_sync_type;
      u32 alarms;
      int m_iRadioInterfacesGraphRefreshInterval;
         // 0...5, same translation to miliseconds as for nGraphRadioRefreshInterval: 10,20,50,100,200,500 ms

      type_vehicle_hardware_interfaces_info hardwareInterfacesInfo;
      type_processes_priorities processesPriorities;

      
      // Radio interfaces order is given by physical order. Can't be changed.
      // Radio links order is auto/user defined.
      // Radio interfaces assignment to radio links is auto/user defined.
      // Radio interfaces parameters are checked/re-computed when the vehicle starts.

      type_radio_interfaces_parameters radioInterfacesParams;
      type_radio_links_parameters radioLinksParams;
      type_logging_parameters loggingParams;
      bool enableDHCP;

      u32 camera_rc_channels;
           // each byte: pitch, roll, yaw camera RC channel (5 bits, 1 based index, 0 - unused), bit 6: 1 - absolute move, 0 - relative move;
           // 4th byte: bit 0...4: 5 bits for speed for relative move;
           // 4th byte: bit 5: relative(0)/absolute(1) move
           // last 2 bits should be 1 for consistency check

      u32 enc_flags;
      
      type_vehicle_stats_info m_Stats;

      int iGPSCount;
      
      type_camera_parameters camera_params[MODEL_MAX_CAMERAS];
      int iCameraCount;
      int iCurrentCamera;

      video_parameters_t video_params;
      type_video_link_profile video_link_profiles[MAX_VIDEO_LINK_PROFILES];
      osd_parameters_t osd_params;
      rc_parameters_t rc_params;
      telemetry_parameters_t telemetry_params;
      audio_parameters_t audio_params;
      type_functions_parameters functions_params;
      type_relay_parameters relay_params;
      type_alarms_parameters alarms_params;

      bool reloadIfChanged(bool bLoadStats);
      bool loadFromFile(const char* filename, bool bLoadStats = false);
      bool saveToFile(const char* filename, bool isOnController);
      int  getLoadedFileVersion();
      bool isRunningOnOpenIPCHardware();
      bool isRunningOnPiHardware();
      bool isRunningOnRadxaHardware();
      void populateHWInfo();
      bool populateVehicleSerialPorts();
      void resetRadioLinkDataRatesAndFlags(int iRadioLink);
      void addNewRadioLinkForRadioInterface(int iRadioInterfaceIndex, bool* pbDefault24Used, bool* pbDefault24_2Used, bool* pbDefault58Used, bool* pbDefault58_2Used);
      void populateRadioInterfacesInfoFromHardware();
      void populateDefaultRadioLinksInfoFromRadioInterfaces();
      void updateRadioInterfacesRadioFlagsFromRadioLinksFlags();
      bool check_update_radio_links();
      void resetToDefaults(bool generateId);
      void resetAudioParams();
      void resetHWCapabilities();
      void resetRadioLinksParams();
      void resetOSDFlags();
      void resetTelemetryParams();
      void resetRCParams();
      void resetVideoParamsToDefaults();
      void resetVideoLinkProfiles(int iProfile = -1);
      void resetVideoLinkProfilesToDataRates(int iUsableDataRateHQ, int iUsableDatarateHP);
      void resetCameraToDefaults(int iCameraIndex);
      void resetCameraProfileToDefaults(camera_profile_parameters_t* pCamParams);
      void resetFunctionsParamsToDefaults();
      void resetRelayParamsToDefaults(type_relay_parameters* pRelayParams);

      void logVehicleRadioInfo();
      int logVehicleRadioLinkDifferences(type_radio_links_parameters* pData1, type_radio_links_parameters* pData2);
      
      bool find_and_validate_camera_settings();
      bool validate_fps_and_exposure_settings(type_video_link_profile* pVideoLinkProfile, camera_profile_parameters_t* pCameraProfile);
      bool validate_settings();
      bool validate_relay_links_flags();
      void validate_radio_flags();

      int getRadioInterfaceIndexForRadioLink(int iRadioLink);
      bool canSwapEnabledHighCapacityRadioInterfaces();
      bool swapEnabledHighCapacityRadioInterfaces();
      int getLastSwappedRadioInterface1();
      int getLastSwappedRadioInterface2();
      bool rotateRadioLinksOrder();

      u32 getRadioLinkVideoDataRateBSP(int iLinkId);
      int getRadioLinkDownlinkDataRate(int iLinkId);
      int getRadioLinkUplinkDataRate(int iLinkId);
      bool radioInterfaceIsWiFiRadio(int iRadioInterfaceIndex);
      bool radioLinkIsWiFiRadio(int iRadioLinkIndex);
      bool radioLinkIsSiKRadio(int iRadioLinkIndex);
      bool radioLinkIsELRSRadio(int iRadioLinkIndex);
      int hasRadioCardsRTL8812AU();
      int hasRadioCardsRTL8812EU();
      int hasRadioCardsAtheros();
      
      bool hasCamera();
      char* getCameraName(int iCameraIndex);
      bool setCameraName(int iCameraIndex, const char* szCamName);
      u32 getActiveCameraType();
      bool isActiveCameraHDMI();
      bool isActiveCameraVeye();
      bool isActiveCameraVeye307();
      bool isActiveCameraVeye327290();
      bool isActiveCameraCSICompatible();
      bool isActiveCameraCSI();
      bool isActiveCameraOpenIPC();
      void log_camera_profiles_differences(camera_profile_parameters_t* pCamProfile1, camera_profile_parameters_t* pCamProfile2, int iIndex1, int iIndex2);
      bool isVideoLinkFixedOneWay();
      int getInitialKeyframeIntervalMs(int iVideoProfile);
      void setDefaultVideoBitrate();
      
      void getCameraFlags(char* szCameraFlags);
      // To fix
      void getVideoFlags(char* szVideoFlags, int iVideoProfile);//, shared_mem_video_link_overwrites* pVideoOverwrites);
      void populateVehicleTelemetryData_v4(t_packet_header_ruby_telemetry_extended_v4* pPHRTE);
      void populateFromVehicleTelemetryData_v3(t_packet_header_ruby_telemetry_extended_v3* pPHRTE);
      void populateFromVehicleTelemetryData_v4(t_packet_header_ruby_telemetry_extended_v4* pPHRTE);

      void copy_video_link_profile(int from, int to);
      int get_video_profile_total_levels(int iProfile);
      int get_video_profile_from_total_levels_shift(int iLevelShift);
      int get_video_profile_level_shift_from_total_levels_shift(int iTotalLevelsShift);
      int get_video_profile_ec_scheme(int iVideoProfile, int* piData, int* piEC);
      int get_level_shift_ec_scheme(int iTotalLevelsShift, int* piData, int* piEC);
      int get_current_max_video_packets_for_all_profiles();

      bool isAudioCapableAndEnabled();
      void constructLongName();
      const char* getShortName();
      const char* getLongName();
      u32 getVehicleFirmwareType();
      static const char* getVehicleType(u8 vtype);
      const char* getVehicleTypeString();
      int getSaveCount();
      void get_rc_channel_name(int nChannel, char* szOutput);

      void updateStatsMaxCurrentVoltage(t_packet_header_fc_telemetry* pPHFCT);
      void updateStatsEverySecond(t_packet_header_fc_telemetry* pPHFCT);

      void copy_radio_link_params(int iFrom, int iTo);
      void copy_radio_interface_params(int iFrom, int iTo);

   private:
      char vehicle_long_name[256];
      int iLoadedFileVersion;
      int iSaveCount;

      void generateUID();
      bool loadVersion8(FILE* fd);
      bool loadVersion9(FILE* fd); // from 7.4
      bool loadVersion10(FILE* fd); // from 7.6
      bool saveVersion10(FILE* fd, bool isOnController); // from 7.6
};

const char* model_getShortFlightMode(u8 mode);
const char* model_getLongFlightMode(u8 mode);
const char* model_getCameraProfileName(int profileIndex);

bool IsModelRadioConfigChanged(type_radio_links_parameters* pRadioLinks1, type_radio_interfaces_parameters* pRadioInterfaces1, type_radio_links_parameters* pRadioLinks2, type_radio_interfaces_parameters* pRadioInterfaces2);

u32 get_sw_version_major(Model* pModel);
u32 get_sw_version_minor(Model* pModel);
u32 get_sw_version_build(Model* pModel);
int is_sw_version_atleast(Model* pModel, int iMajor, int iMinor);
