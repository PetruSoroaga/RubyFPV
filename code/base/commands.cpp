/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "base.h"
#include "config.h"
#include "commands.h"
#include "../radio/radiopackets2.h"

const char* commands_get_description(u8 command_type)
{
   static char szCommandDesc[64];
   szCommandDesc[0] = 0;

   command_type = command_type & COMMAND_TYPE_MASK;

   switch (command_type)
   {
      case COMMAND_ID_SET_VEHICLE_TYPE: strcpy(szCommandDesc, "Set_Vehicle_Type"); break;
      case COMMAND_ID_SET_RADIO_LINK_FREQUENCY: strcpy(szCommandDesc, "Set_Radio_Link_Frequency"); break;
      case COMMAND_ID_SET_RADIO_LINK_CAPABILITIES: strcpy(szCommandDesc, "Set_Radio_Link_Capabilities"); break;
      case COMMAND_ID_SET_RADIO_LINK_FLAGS: strcpy(szCommandDesc, "Set_Radio_Link_Flags"); break;
      case COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION: strcpy(szCommandDesc, "RadioLink_Flags_Changed_Confirmation"); break;
      case COMMAND_ID_SET_RADIO_CARD_MODEL: strcpy(szCommandDesc, "Set_Radio_Card_Model"); break;
      case COMMAND_ID_SET_MODEL_FLAGS: strcpy(szCommandDesc, "Set_Model_Flags"); break;
      case COMMAND_ID_GET_USB_INFO: strcpy(szCommandDesc, "Get_USB_Info"); break;
      case COMMAND_ID_GET_USB_INFO2: strcpy(szCommandDesc, "Get_USB_Info2"); break;
      case COMMAND_ID_SET_CAMERA_PROFILE: strcpy(szCommandDesc, "Set_Camera_Profile"); break;
      case COMMAND_ID_SET_CAMERA_PARAMETERS: strcpy(szCommandDesc, "Set_Camera_Params"); break;
      case COMMAND_ID_SET_CURRENT_CAMERA: strcpy(szCommandDesc, "Set_Current_Camera"); break;
      case COMMAND_ID_SET_VIDEO_H264_QUANTIZATION: strcpy(szCommandDesc, "Set_VideoH264_Quantization"); break;
      case COMMAND_ID_FORCE_CAMERA_TYPE: strcpy(szCommandDesc, "Set_Force_Camera_Type"); break;
      case COMMAND_ID_SET_OSD_CURRENT_LAYOUT: strcpy(szCommandDesc, "Set_Current_OSD_Layout"); break;
      case COMMAND_ID_GET_CORE_PLUGINS_INFO: strcpy(szCommandDesc, "Get_Core_Plugins_Info"); break;
      case COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS: strcpy(szCommandDesc, "Set_Controller_Telemetry_Options"); break;

      case COMMAND_ID_SWAP_RADIO_INTERFACES: strcpy(szCommandDesc, "Swap_Radio_Interfaces"); break;
      case COMMAND_ID_SET_RELAY_PARAMETERS: strcpy(szCommandDesc, "Set_Relay_Parameters"); break;
      case COMMAND_ID_SET_SERIAL_PORTS_INFO: strcpy(szCommandDesc, "Set_Serial_Ports_Info"); break;
      case COMMAND_ID_SET_AUDIO_PARAMS: strcpy(szCommandDesc, "Set_Audio_Params"); break;
      case COMMAND_ID_RESET_ALL_TO_DEFAULTS: strcpy(szCommandDesc, "ResetToDefaults"); break;
      case COMMAND_ID_SET_VIDEO_PARAMS: strcpy(szCommandDesc, "Set_Video_Params"); break;
      case COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES: strcpy(szCommandDesc, "Update_Video_Link_Profiles"); break;
      case COMMAND_ID_RESET_VIDEO_LINK_PROFILE: strcpy(szCommandDesc, "Reset_Video_Link_Profile"); break;
      case COMMAND_ID_SET_TELEMETRY_PARAMETERS: strcpy(szCommandDesc, "Set_Telemetry_Params"); break;
      case COMMAND_ID_SET_TX_POWERS: strcpy(szCommandDesc, "Set_TxPowers"); break;
      case COMMAND_ID_SET_GPS_INFO: strcpy(szCommandDesc, "Set_GPS_Info"); break;
      case COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS: strcpy(szCommandDesc, "Set_Functions_Triggers_Params"); break;
      case COMMAND_ID_SET_RC_PARAMS: strcpy(szCommandDesc, "Set_RcParams"); break;
      case COMMAND_ID_SET_VEHICLE_NAME: strcpy(szCommandDesc, "Set_VehicleName"); break;
      case COMMAND_ID_REBOOT: strcpy(szCommandDesc, "Reboot"); break;
      case COMMAND_ID_SET_NICE_VALUE_TELEMETRY: strcpy(szCommandDesc, "Set_NiceValueTelemetry"); break;
      case COMMAND_ID_SET_NICE_VALUES: strcpy(szCommandDesc, "Set_NiceValues"); break;
      case COMMAND_ID_SET_IONICE_VALUES: strcpy(szCommandDesc, "Set_IONiceValues"); break;
      case COMMAND_ID_SET_RADIO_SLOTTIME: strcpy(szCommandDesc, "Set_Radio_Slottime"); break;
      case COMMAND_ID_SET_RADIO_THRESH62: strcpy(szCommandDesc, "Set_Radio_Thresh62"); break;
      case COMMAND_ID_SET_ENABLE_DHCP: strcpy(szCommandDesc, "Set_Enable_DHCP"); break;
      case COMMAND_ID_SET_ALL_PARAMS: strcpy(szCommandDesc, "Set_All_Params"); break;
      case COMMAND_ID_GET_ALL_PARAMS_ZIP: strcpy(szCommandDesc, "Get_All_Params_Zip"); break;
      case COMMAND_ID_SET_OSD_PARAMS: strcpy(szCommandDesc, "SetOSDParams"); break;
      case COMMAND_ID_SET_ALARMS_PARAMS: strcpy(szCommandDesc, "SetAlarmsParams"); break;
      case COMMAND_ID_GET_CURRENT_VIDEO_CONFIG: strcpy(szCommandDesc, "Get_Current_Video_Config"); break;
      case COMMAND_ID_GET_MODULES_INFO: strcpy(szCommandDesc, "Get_Modules_Info"); break;
      case COMMAND_ID_GET_MEMORY_INFO: strcpy(szCommandDesc, "Get_Memory_Info"); break;
      case COMMAND_ID_GET_CPU_INFO: strcpy(szCommandDesc, "Get_CPU_Info"); break;
      case COMMAND_ID_SET_RC_CAMERA_PARAMS: strcpy(szCommandDesc, "Set_Camera_RC_Params"); break;
      case COMMAND_ID_ENABLE_LIVE_LOG: strcpy(szCommandDesc, "Enable_Live_Log"); break;
      case COMMAND_ID_UPLOAD_SW_TO_VEHICLE: strcpy(szCommandDesc, "Upload_SW_To_Vehicle"); break;
      case COMMAND_ID_UPLOAD_SW_TO_VEHICLE63: strcpy(szCommandDesc, "Upload_SW_To_Vehicle_2"); break;
      case COMMAND_ID_UPLOAD_FILE_SEGMENT: strcpy(szCommandDesc, "Upload_File_Segment"); break;
      case COMMAND_ID_SET_CLOCK_SYNC_TYPE: strcpy(szCommandDesc, "Set_Clock_Sync_Type"); break;
      case COMMAND_ID_RESET_CPU_SPEED: strcpy(szCommandDesc, "Reset_CPU_Speed"); break;
      case COMMAND_ID_SET_DEVELOPER_FLAGS: strcpy(szCommandDesc, "Set_Developer_Flags"); break;
      case COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS: strcpy(szCommandDesc, "Reset_All_Developer_Flags"); break;
      case COMMAND_ID_ENABLE_DEBUG: strcpy(szCommandDesc, "Enable_Debug"); break;
      case COMMAND_ID_DEBUG_GET_TOP: strcpy(szCommandDesc, "Dbg_Get_TOP"); break;
      case COMMAND_ID_SET_ENCRYPTION_PARAMS: strcpy(szCommandDesc, "Set Encryption Params"); break;
      case COMMAND_ID_DOWNLOAD_FILE: strcpy(szCommandDesc, "Download_File"); break;
      case COMMAND_ID_DOWNLOAD_FILE_SEGMENT: strcpy(szCommandDesc, "Download_File_Segment"); break;
      case COMMAND_ID_CLEAR_LOGS: strcpy(szCommandDesc, "Clear_Logs"); break;
       
      case COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_LOW: strcpy(szCommandDesc, "Manual switch to video link low quality"); break;
      case COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_MED: strcpy(szCommandDesc, "Manual switch to video link med quality"); break;
      case COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_NORMAL: strcpy(szCommandDesc, "Manual switch to video link normal quality"); break;
      case COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_AUTO: strcpy(szCommandDesc, "Manual switch to auto video link quality"); break;

      default:
         sprintf(szCommandDesc, "%d", command_type);
         break;
   }
   return szCommandDesc;
}
