#pragma once

#define LOG_USE_PROCESS "config/use_log_process"
#define LOG_FILE_START "logs/log_start.txt"
#define LOG_FILE_SYSTEM "logs/log_system.txt"
#define LOG_FILE_ERRORS "logs/log_errors.txt"
#define LOG_FILE_ERRORS_SOFT "logs/log_errors_soft.txt"
#define LOG_FILE_COMMANDS "logs/log_commands.txt"
#define LOG_FILE_WATCHDOG "logs/log_watchdog.txt"
#define LOG_FILE_VIDEO "logs/log_video.txt"
#define LOG_FILE_CAPTURE_VEYE "logs/log_capture_veye.txt"
#define LOG_FILE_VEHICLE "logs/log_vehicle_%s.txt"


#define FOLDER_RUBY "/home/pi/ruby"
#define FOLDER_CONFIG "config/"
#define FOLDER_MEDIA "media/"
#define FOLDER_MEDIA_VEHICLE_DATA "media/vehicle-%u"
#define FOLDER_VEHICLE_HISTORY "config/models/history-%d"
#define FOLDER_OSD_PLUGINS "plugins/osd/"
#define FOLDER_CORE_PLUGINS "plugins/core/"

#define FILE_UPDATE_CMD_LIST "ruby_update_op.cmd"
#define FILE_INFO_VERSION "version_ruby_base.txt"
#define FILE_INFO_LAST_UPDATE "ruby_update.log"

#define FILE_FORCE_VEHICLE "/boot/forcevehicle"
#define FILE_FORCE_VEHICLE_NO_CAMERA "force_no_camera"
#define FILE_FORCE_ROUTER "/boot/forcerouter"
#define FILE_FORCE_RESET "/boot/forcereset"


#define FILE_HARDWARE_I2C_DEVICES "config/i2c_devices_settings.cfg"
#define FILE_PREFERENCES "config/ui_preferences.cfg"
#define FILE_OSD_PLUGINS_SETTINGS "config/osd_plugins_settings.cfg"
#define FILE_CORE_PLUGINS_SETTINGS "config/core_plugins_settings.cfg"
#define FILE_CONTROLLER_SETTINGS "config/controller_settings.cfg"
#define FILE_CONTROLLER_INTERFACES "config/controller_interfaces.cfg"
#define FILE_CONTROLLER_ID "config/controller_id.cfg"
#define FILE_CONTROLLER_BUTTONS "config/controller_btns.cfg"
#define FILE_CONTROLLER_OSD_WIDGETS "config/osd_widgets.cfg"

#define FILE_CONFIG_CURRENT_VERSION "config/current_version.inf"
#define FILE_VEHICLE_SETTINGS "config/vehicle_settings.cfg"

#define FILE_FIRST_BOOT "config/firstboot.txt"
#define FILE_FIRST_PAIRING_DONE "config/firstpairingdone"
#define FILE_SYSTEM_TYPE "config/system_type.txt"
#define FILE_VEHICLE_SPECTATOR "config/models/spect-%d.mdl"
#define FILE_VEHICLE_CONTROLLER "config/models/ctrl-%d.mdl"
#define FILE_ACTIVE_CONTROLLER_MODEL "config/active_controller_model.cfg"
#define FILE_CURRENT_VEHICLE_MODEL "config/current_vehicle.mdl"
#define FILE_CURRENT_VEHICLE_MODEL_BACKUP "config/current_vehicle.bak"
#define FILE_CURRENT_VEHICLE_COUNT "config/current_vehicle_count.cfg"
#define FILE_CURRENT_SEARCH_BAND "config/current_search_band.cfg"
#define FILE_CURRENT_RADIO_HW_CONFIG "config/current_radios.cfg"
#define FILE_ENCRYPTION_PASS "config/current_pph.cfg"
#define FILE_CONFIG_HW_SERIAL_PORTS "config/hw_serial.cfg"
#define FILE_CONFIG_MODELS_CONNECT_FREQUENCIES "config/models_connect_freq.cfg"
#define FILE_CONFIG_LAST_SIK_RADIOS_DETECTED "config/last_sik_radios_detected.cfg"

#define FILE_BOOT_TIMESTAMP "config/boot_timestamp.cfg"
#define FILE_BOOT_COUNT "config/boot_count.cfg"
#define FILE_VEHICLE_REBOOT_CACHE "config/reboot_cache.tmp"

#define FILE_FORMAT_SCREENSHOT "picture-%s-%d-%d-%d.png"
#define FILE_FORMAT_VIDEO_INFO "video-%s-%d-%d-%d.info"


//#define VIDEO_RECORDER_COMMAND "raspivid"
//#define VIDEO_RECORDER_COMMAND_VEYE "/usr/local/bin/veye_raspivid"
//#define VIDEO_RECORDER_COMMAND_VEYE307 "/usr/local/bin/307/veye_raspivid"
//#define VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME "veye_raspivid"
#define VIDEO_RECORDER_COMMAND "ruby_capture_raspi"
#define VIDEO_RECORDER_COMMAND_VEYE "./ruby_capture_veye"
#define VIDEO_RECORDER_COMMAND_VEYE307 "./ruby_capture_veye"
#define VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME "ruby_capture_veye"

#define VIDEO_PLAYER_PIPE "ruby_player_p"
#define VIDEO_PLAYER_STREAM "ruby_player_s"
#define VIDEO_PLAYER_OFFLINE "ruby_player_f2"

#define VEYE_COMMANDS_FOLDER "/usr/local/share/veye-raspberrypi"
#define VEYE_COMMANDS_FOLDER307 "/usr/local/share/veye-raspberrypi/307"


// Temporary files (for flags, logic)

#define FOLDER_RUBY_TEMP "/home/pi/ruby/tmp"
#define FOLDER_USB_MOUNT "tmp/tmpFiles"

#define TEMP_USB_TETHERING_DEVICE "tmp/usb_tethering"

#define TEMP_VIDEO_MEM_FOLDER "tmp/memdisk"
#define TEMP_VIDEO_MEM_FILE "tmp/memdisk/tmpVideo.h264"
#define TEMP_VIDEO_FILE "tmp/tmpVideo.h264"
#define TEMP_VIDEO_FILE_INFO "tmp/tmpVideo.info"
#define TEMP_VIDEO_FILE_PROCESS_ERROR "tmp/tmpErrorVideo.stat"

#define FILE_TMP_UPDATE_IN_PROGRESS "tmp/updateinprogress"
#define FILE_TMP_REINIT_RADIO_IN_PROGRESS "tmp/radioreinitinprogress"
#define FILE_TMP_REINIT_RADIO_REQUEST "tmp/radioreinitrequest"
#define FILE_TMP_ARMED "tmp/armed"
#define FILE_TMP_RC_DETECTED "tmp/rclink"
#define FILE_TMP_ALARM_ON "tmp/alarmon"
#define FILE_TMP_I2C_UPDATED "tmp/i2c_updated"
#define FILE_TMP_CONTROLLER_LOCAL_STATS "config/tmp_local_stats.inf"
#define FILE_TMP_CONTROLLER_LOAD_LOCAL_STATS "config/tmp_load_local_stats"
#define FILE_TMP_CONTROLLER_CENTRAL_CRASHED "config/tmp_central_crashed"
#define FILE_TMP_CONTROLLER_PAUSE_WATCHDOG "tmp/pausecontrollerwatchdog"
#define FILE_TMP_HDMI_CHANGED "config/tmp_hdmi_changed"
#define FILE_TMP_CAMERA_NAME "tmp/cam_name.txt"
#define FILE_TMP_CURRENT_VIDEO_PARAMS "tmp/current_video_config.txt"
#define FILE_TMP_SIK_CONFIG_FINISHED "tmp/sik_config_complete.txt"
#define FILE_TMP_AUDIO_RECORDING "tmp/audio.wav"

#define FILE_ID_VEHICLE_LOG 0
#define FILE_ID_VEHICLE_LOGS_ARCHIVE 1
#define FILE_ID_CORE_PLUGINS_ARCHIVE 5