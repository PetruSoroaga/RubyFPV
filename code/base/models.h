#pragma once
#include "base.h"
#include "hardware.h"
#include "flags.h"
#include "config_rc.h"
#include "shared_mem.h"
#include "../radio/radiopackets2.h"

#define MODEL_TYPE_GENERIC 0
#define MODEL_TYPE_DRONE 1
#define MODEL_TYPE_AIRPLANE 2
#define MODEL_TYPE_HELI 3
#define MODEL_TYPE_CAR 4
#define MODEL_TYPE_BOAT 5
#define MODEL_TYPE_ROBOT 6
#define MODEL_TYPE_RELAY 7
#define MODEL_TYPE_LAST 8

#define MODEL_TELEMETRY_TYPE_NONE 0
#define MODEL_TELEMETRY_TYPE_MAVLINK 1
#define MODEL_TELEMETRY_TYPE_LTM 2

#define MODEL_MAX_CAMERAS 4
#define MODEL_CAMERA_PROFILES 3

#define MODEL_MAX_OSD_PROFILES 5

#define CAMERA_FLAG_FORCE_MODE_1 1
#define CAMERA_FLAG_AWB_MODE_OLD ((u32)(((u32)0x01)<<1))


typedef struct
{
   u32 flags;
   u8 flip_image;
   u8 brightness;
   u8 contrast;
   u8 saturation;
   u8 sharpness;
   u8 exposure;
   u8 whitebalance;
   u8 metering;
   u8 drc; // stands for AGC for IMX327 camera
   float analogGain;
   float awbGainB;
   float awbGainR;
   float fovV;
   float fovH;
   u8 vstab; // on/off
   u8 ev;    // -10 to 10, 0 default, translated to 1...21, default 11
   u16 iso; // 100 - 800, 0 for off
   u16 shutterspeed; // in 1/x of a second, 0 for off, min is 30, max is 30000 (1/30 to 1/30000);
   u8 wdr; // used for IMX327 camera for WDR mode
   u8 dayNightMode; // 0 - day mode, 1 - night mode, only for Veye cameras
   u8 dummy[2];
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
   int videoAdjustmentStrength; // 1..10 (from 10% to 100% strength)
   u32 lowestAllowedAdaptiveVideoBitrate;
   u32 uMaxAutoKeyframeInterval;
   u32 uExtraFlags; // bit 0: Fill H264 SPS timings 
   u32 dummy[3];
} video_parameters_t;


typedef struct
{
   u32 flags;
      // bit x: not used now

   u32 encoding_extra_flags; // same as for radio video packet encoding_extra_flags

   // byte 0:
   //    bit 0..2  - scramble blocks count
   //    bit 3     - enables feature: restransmission of missing packets
   //    bit 4     - signals that current video is on reduced video bitrate due to tx time overload on vehicle
   //    bit 5     - enable adaptive video link params
   //    bit 6     - use controller info too when adjusting video link params
   //    bit 7     - go lower adaptive video profile when controller link lost

   // byte 1:   - max time to wait for retransmissions (in ms*5)// affects rx buffers size
   // byte 2:   - retransmission duplication percent (0-100%), 0xFF = auto, bit 0..3 - regular packets duplication, bit 4..7 - retransmitted packets duplication
   // byte 3:
   //    bit 0  - use medium adaptive video
   //    bit 1  - enable video auto quantization
   //    bit 2  - video auto quantization strength

   int radio_datarate_video; // 0 - to use auto datarate
   int radio_datarate_data;  // 0 - to use auto datarate
   u32 radio_flags; // 0 if no custom ones, use the radio link radio flags
   int h264profile; // 0 = baseline, 1 = main, 2 = high
   int h264level; //0 = 4.0, 1 = 4.1, 2 = 4.2
   int h264refresh; // 0 = cyclic, 1 = adaptive, 2 = both, 3 = cyclicrows
   int h264quantization; // 0 - auto, // pozitive - value to use, // negative - value when disabled (auto)
   u8 insertPPS;

   int width;
   int height;
   int block_packets;
   int block_fecs;
   int packet_length;
   int keyframe; // positive: fixed keyframe, negative: auto keyframe interval (negative value is the stored fixed one)
   int fps;
   u32 bitrate_fixed_bps; // in bits per second, 0 for auto

} type_video_link_profile;


typedef struct
{
   int layout;
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
     // byte 1: transparency (0...3)
     // byte 2: bit 0...3  osd stats windows font size (0...6)
     //         bit 4...7  osd stats background transparency (0...3)
     // byte 3:
     //    bit 0: show controller link lost alarm
     //
} osd_parameters_t;


#define RELAY_FLAGS_NONE 0
#define RELAY_FLAGS_ALL 0xFFFFFFFF
#define RELAY_FLAGS_TELEMETRY ((u32)1)
#define RELAY_FLAGS_VIDEO (((u32)1)<<1)
#define RELAY_FLAGS_COMMANDS (((u32)1)<<2)
#define RELAY_FLAGS_SHOW_OSD (((u32)1)<<3)

#define RELAY_MODE_NONE       0
#define RELAY_MODE_REMOTE     (((u32)1))
#define RELAY_MODE_PIP_MAIN   (((u32)1)<<1)
#define RELAY_MODE_PIP_REMOTE (((u32)1)<<2)

typedef struct
{
   int isRelayEnabledOnRadioLinkId; // 0 - disabled, positive: radioLinkId - 1 
   u8  uCurrentRelayMode;
   u32 uRelayFrequency;
   u32 uRelayVehicleId;
   u32 uRelayFlags;
} type_relay_parameters;

typedef struct
{
   bool rc_enabled;
   int rc_frames_per_second;
   int rc_failsafe_timeout_ms;
   bool dummy1;
   int receiver_type;
   int inputType; // RC input type on the controller: 0 none, 1 usb, 2 ibus, 3 sbus
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

   u32 failsafeFlags; // first byte: what type of failsafe to do, globally;
                      // 2nd-3rd byte: failsafe value (for that type of failsafe)
   u32 channelsCount;
   u32 hid_id; // USB HID id on the controller for RC control
   u32 flags;
          // bit 0: output to FC enabled
   u32 rcChAssignmentThrotleReverse;
   u32 dummy[9];
} rc_parameters_t;


#define TELEMETRY_TYPE_NONE 0
#define TELEMETRY_TYPE_MAVLINK 1
#define TELEMETRY_TYPE_LTM 2
#define TELEMETRY_TYPE_TRANSPARENT 10

typedef struct
{
   int fc_telemetry_type; // 0 = None, 1 = MAVLink, 2 == LTM

   int dummy1;
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
   u32 flags; // 0..7 data packets, 8...15 fec packets
} audio_parameters_t;


#define MAX_MODEL_I2C_BUSSES 6
#define MAX_MODEL_I2C_DEVICES 16
#define MAX_MODEL_SERIAL_BUSSES 6
#define MAX_MODEL_SERIAL_BUS_NAME 16

typedef struct
{
   int i2c_bus_count;
   int i2c_device_count;
   int i2c_bus_numbers[MAX_MODEL_I2C_BUSSES];
   int i2c_devices_address[MAX_MODEL_I2C_DEVICES];
   int i2c_devices_bus[MAX_MODEL_I2C_DEVICES];
   int radio_interface_count;

   int serial_bus_count;
   char serial_bus_names[MAX_MODEL_SERIAL_BUSSES][MAX_MODEL_SERIAL_BUS_NAME];
   u32 serial_bus_supported_and_usage[MAX_MODEL_SERIAL_BUSSES];
     // byte 0: usage type
     // byte 1: bits 0...3 usb or hardware port index
     //         bits 4 : 0 - hardware builtin, 1 - on usb
     //         bits 5 : supported 0/1
   int serial_bus_speed[MAX_MODEL_SERIAL_BUSSES];
} model_hardware_info_t;


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
   int txPower;
   int txPowerAtheros;
   int txPowerRTL;
   int txPowerSiK;
   int txMaxPower;
   int txMaxPowerAtheros;
   int txMaxPowerRTL;
   int slotTime;
   int thresh62;

   int  interfaces_count;
   int  interface_card_model[MAX_RADIO_INTERFACES]; // 0 or positive - autodetected, negative - user set
   int  interface_link_id[MAX_RADIO_INTERFACES];
   int  interface_power[MAX_RADIO_INTERFACES];
   u32  interface_type_and_driver[MAX_RADIO_INTERFACES]; // first byte: radio type, second byte = driver type, third byte: supported card flag
   u8   interface_supported_bands[MAX_RADIO_INTERFACES];  // bits 0-3: 2.3, 2.4, 2.5, 5.8 bands
   char interface_szMAC[MAX_RADIO_INTERFACES][MAX_MAC_LENGTH];
   char interface_szPort[MAX_RADIO_INTERFACES][6]; // first byte - first char, sec byte - sec char
   u32  interface_capabilities_flags[MAX_RADIO_INTERFACES]; // what the card is used for: video/data/relay/tx/rx
   u32  interface_current_frequency[MAX_RADIO_INTERFACES]; // current frequency for this card
   u32  interface_current_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: frame type, STBC, LDP, MCS etc
   int  interface_datarates[MAX_RADIO_INTERFACES][2]; // radio data rates [0] - video, [1] - data, 0 - auto (use radio link datarates), positive: legacy datarates, negative (-1 or less): MCS rate
} type_radio_interfaces_parameters;


typedef struct
{
   int links_count;
   u32 link_frequency[MAX_RADIO_INTERFACES];
   u32 link_capabilities_flags[MAX_RADIO_INTERFACES]; // data/video/both?
   u32 link_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: frame type, STBC, LDP, MCS etc
   int link_datarates[MAX_RADIO_INTERFACES][2]; // radio data rates [0] - video, [1] - data, negative (-1 or less): MCS rate

   u8  bUplinkSameAsDownlink[MAX_RADIO_INTERFACES];
   u32 uplink_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: frame type, STBC, LDP, MCS etc
   int uplink_datarates[MAX_RADIO_INTERFACES][2]; // radio data rates [0] - video, [1] - data, negative (-1 or less): MCS rate

   u32 dummy[12];
} type_radio_links_parameters;

typedef struct
{
   u8 uAlarmMotorCurrentThreshold; // in 0.1 amp increments, most significant bit: 0 - alarm disabled, 1 - alarm enabled

} type_alarms_parameters;

class Model
{
   public:
      Model();

      bool b_mustSyncFromVehicle; // non persistent
      bool bDeveloperMode;
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

      int board_type;
      char vehicle_name[MAX_VEHICLE_NAME_LENGTH];
      u32 vehicle_id;
      u32 controller_id;
      u32 sw_version; // byte 0: minor version, byte 1 Major version, byte 2-3: build nb
      bool is_spectator;
      u8 vehicle_type;
      int clock_sync_type;
      u32 alarms;

      model_hardware_info_t hardware_info;

      // radio interfaces parameters are checked/re-computed when the vehicle starts.

      type_radio_interfaces_parameters radioInterfacesParams;
      type_radio_links_parameters radioLinksParams;

      bool enableDHCP;

      u32 camera_rc_channels;
           // each byte: pitch, roll, yaw camera RC channel (5 bits, 1 based index, 0 - unused), bit 6: 1 - absolute move, 0 - relative move;
           // 4th byte: bit 0...4: 5 bits for speed for relative move;
           // 4th byte: bit 5: relative(0)/absolute(1) move
           // last 2 bits should be 1 for consistency check

      u32 enc_flags;
      
      type_vehicle_stats_info m_Stats;

      int iGPSCount;
      
      int niceRC;
      int niceRouter;
      int ioNiceRouter;
      int niceTelemetry;
      int niceVideo;
      int niceOthers;
      int ioNiceVideo; // 0 or negative - disabled;
      int iOverVoltage; // 0 or negative - disabled, negative - default value
      int iFreqARM; // 0 or negative - disabled
      int iFreqGPU; // 0 or negative - disabled
 
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
      int getLoadedFileVersion();
      void populateHWInfo();
      bool populateVehicleSerialPorts();
      void populateRadioInterfacesInfoFromHardware();
      void populateDefaultRadioLinksInfoFromRadioInterfaces();
      bool check_update_radio_links();
      void resetToDefaults(bool generateId);
      void resetRadioLinksParams();
      void resetOSDFlags();
      void resetTelemetryParams();
      void resetRCParams();
      void resetVideoParamsToDefaults();
      void resetVideoLinkProfiles(int iProfile = -1);
      void resetCameraToDefaults(int iCameraIndex);
      void resetCameraProfileToDefaults(camera_profile_parameters_t* pCamParams);
      void resetFunctionsParamsToDefaults();
      void resetRelayParamsToDefaults();

      void logVehicleRadioInfo();
      bool validate_camera_settings();
      bool validate_settings();
      void updateRadioInterfacesRadioFlags();
      u32 getLinkRealDataRate(int nLinkId);
      int getRadioInterfaceIndexForRadioLink(int iRadioLink);
      void swapRadioInterfaces();
      bool radioLinkIsSiKRadio(int iRadioLinkIndex);

      bool hasCamera();
      char* getCameraName(int iCameraIndex);
      void setCameraName(int iCameraIndex, const char* szCamName);
      u32 getCameraType();
      bool isCameraHDMI();
      bool isCameraVeye();
      bool isCameraVeye307();
      bool isCameraVeye327290();
      bool isCameraCSICompatible();

      void setDefaultVideoBitrate();
      
      void setAWBMode();
      void getCameraFlags(char* szCameraFlags);
      void getVideoFlags(char* szVideoFlags, int iVideoProfile, shared_mem_video_link_overwrites* pVideoOverwrites);
      void populateVehicleTelemetryData_v2(t_packet_header_ruby_telemetry_extended_v2* pPHRTE);
      void populateFromVehicleTelemetryData_v1(t_packet_header_ruby_telemetry_extended_v1* pPHRTE);
      void populateFromVehicleTelemetryData_v2(t_packet_header_ruby_telemetry_extended_v2* pPHRTE);

      void copy_video_link_profile(int from, int to);
      int get_video_link_profile_max_level_shifts(int iProfile);

      void constructLongName();
      const char* getShortName();
      const char* getLongName();
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
      bool loadVersion8(const char* szFile, FILE* fd);
      bool saveVersion8(const char* szFile, FILE* fd, bool isOnController);
};

const char* model_getShortFlightMode(u8 mode);
const char* model_getLongFlightMode(u8 mode);
const char* model_getCameraProfileName(int profileIndex);
int getModelsSpectatorCount();
Model* getModelSpectator(int index);
Model* addModelSpectator(u32 vehicleId);
void moveModelSpectatorToTop(int index);
bool loadModelsSpectator();
bool saveModelsSpectator();

bool hasModel(u32 uVehicleId);
int getModelsCount();
Model* getModelAtIndex(int index);
void deleteAllModels();
Model* addNewModel();
void replaceModel(int index, Model* pModel);
Model* findModelWithId(u32 vehicle_id);
void deleteModel(Model* pModel);
void loadModels();
void saveModels();
void saveModel(Model* pModel);
void saveAsCurrentModel(Model* pModel);
