#pragma once

#ifdef __cplusplus
extern "C" {
#endif 


void str_sanitize_modelname(char* szName);
void str_sanitize_filename(char* szFileName);

char* str_format_time(u32 miliseconds);


void str_getDataRateDescription(int dataRate, char* szOutput);
void str_format_bitrate(u32 bitrate, char* szBuffer);
void str_format_bitrate_no_sufix(u32 bitrate, char* szBuffer);
const char* str_getBandName(u32 band);
char* str_format_frequency(u32 uFrequency);
char* str_format_frequency_no_sufix(u32 uFrequency);

char* str_get_pipe_flags(int iFlags);

const char* str_get_hardware_board_name(u32 board_type);
const char* str_get_hardware_wifi_name(u32 wifi_type);
void str_get_hardware_camera_type_string(u32 camType, char* szOutput);

void str_get_supported_bands_string(u32 bands, char* szOut);

const char* str_get_radio_type_description(int typeAndDriver);
const char* str_get_radio_driver_description(int typeAndDriver);
const char* str_get_radio_card_model_string(int cardModel);
const char* str_get_radio_card_model_string_short(int cardModel);
void str_get_radio_capabilities_description(u32 flags, char* szOutput);

void str_get_radio_frame_flags_description(u32 radioFlags, char* szOutput);

char* str_get_video_profile_name(u32 videoProfileId);

char* str_get_radio_stream_name(int iStreamId);

char* str_get_osd_screen_name(int iOSDId);

char* str_get_serial_port_usage(int iSerialPortUsage);

char* str_get_developer_flags(u32 uDeveloperFlags);

char* str_get_command_response_flags_string(u32 uResponseFlags);

#ifdef __cplusplus
}  
#endif 