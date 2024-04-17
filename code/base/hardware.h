#pragma once
#include "config.h"
#include "hardware_radio.h"
#include "hardware_radio_sik.h"
#include "hardware_i2c.h"
#include "hardware_serial.h"


#define WIFI_TYPE_NONE 0
#define WIFI_TYPE_A 1 // per bits
#define WIFI_TYPE_G 2 // per bits
#define WIFI_TYPE_AG 3 // per bits


#define BOARD_TYPE_NONE     0
#define BOARD_TYPE_PIZERO   1
#define BOARD_TYPE_PIZEROW  2
#define BOARD_TYPE_PIZERO2  3
#define BOARD_TYPE_PI2B     5
#define BOARD_TYPE_PI2BV11  6
#define BOARD_TYPE_PI2BV12  7
#define BOARD_TYPE_PI3APLUS 10
#define BOARD_TYPE_PI3B     15
#define BOARD_TYPE_PI3BPLUS 16
#define BOARD_TYPE_PI4B     20

#define BOARD_TYPE_OPENIPC_GOKE200 30
#define BOARD_TYPE_OPENIPC_GOKE210 31
#define BOARD_TYPE_OPENIPC_GOKE300 32

#define BOARD_TYPE_OPENIPC_SIGMASTER_338Q 40


#define CAMERA_TYPE_NONE 0
#define CAMERA_TYPE_CSI   1
#define CAMERA_TYPE_VEYE290  3
#define CAMERA_TYPE_VEYE307  4
#define CAMERA_TYPE_VEYE327  5
#define CAMERA_TYPE_HDMI  7
#define CAMERA_TYPE_USB   8
#define CAMERA_TYPE_IP    9
#define CAMERA_TYPE_OPENIPC_IMX307 20
#define CAMERA_TYPE_OPENIPC_IMX335 21
#define CAMERA_TYPE_OPENIPC_IMX415 22


#define MAX_JOYSTICK_INTERFACE_NAME 128
#define MAX_JOYSTICK_INTERFACES 4
#define MAX_JOYSTICK_AXES 24
#define MAX_JOYSTICK_BUTTONS 24

#ifdef __cplusplus
extern "C" {
#endif 


typedef struct
{
   int deviceIndex;
   char szName[MAX_JOYSTICK_INTERFACE_NAME];
   u32 uId;
   u8 countAxes;
   u8 countButtons;
   int axesValues[MAX_JOYSTICK_AXES];
   int buttonsValues[MAX_JOYSTICK_BUTTONS];
   int axesValuesPrev[MAX_JOYSTICK_AXES];
   int buttonsValuesPrev[MAX_JOYSTICK_BUTTONS];
   int fd;
} __attribute__((packed)) hw_joystick_info_t;



int init_hardware();
int init_hardware_only_status_led();
void _hardware_load_system_type();

void hardware_release();
void hardware_loop();
void hardware_swap_buttons(int swap);

u32 hardware_getOnlyBoardType();
u32 hardware_getBoardType();
u32 hardware_get_base_ruby_version();

int hardware_board_is_openipc(int iBoardType);
int hardware_board_is_goke(int iBoardType);
int hardware_board_is_sigmastar(int iBoardType);

void hardware_enum_joystick_interfaces();
int hardware_get_joystick_interfaces_count();
hw_joystick_info_t* hardware_get_joystick_info(int index);
int hardware_open_joystick(int joystickIndex);
void hardware_close_joystick(int joystickIndex);
int hardware_read_joystick(int joystickIndex, int miliSec);
int hardware_is_joystick_opened(int joystickIndex);

u16 hardware_get_flags();

void hardware_setCriticalErrorFlag();
void hardware_setRecoverableErrorFlag();
void hardware_flashGreenLed();

int isKeyMenuPressed();
int isKeyBackPressed();
int isKeyPlusPressed();
int isKeyMinusPressed();
int isKeyQA1Pressed();
int isKeyQA2Pressed();
int isKeyQA3Pressed();

int isKeyMenuLongPressed();
int isKeyBackLongPressed();
int isKeyPlusLongPressed();
int isKeyMinusLongPressed();
int isKeyPlusLongLongPressed();
int isKeyMinusLongLongPressed();

void hardware_override_keys(int iKeyMenu, int iKeyBack, int iKeyPlus, int iKeyMinus, int iKeyMenuLong, int iKeyBackLong, int iKeyPlusLong, int iKeyMinusLong);

void hardware_ResetLongPressStatus();
void hardware_blockCurrentPressedKeys();

int hardware_try_mount_usb();
void hardware_unmount_usb();
char* hardware_get_mounted_usb_name();

int hardware_is_station();
int hardware_is_vehicle();
int hardware_is_running_on_openipc();

void hardware_sleep_ms(u32 miliSeconds);
void hardware_sleep_micros(u32 microSeconds);

void hardware_recording_led_set_off();
void hardware_recording_led_set_on();
void hardware_recording_led_set_blinking();

void hardware_mount_root();
void hardware_mount_boot();
int hardware_get_free_space_kb();

int hardware_has_eth();

int hardware_get_cpu_speed(); // in Mhz
int hardware_get_gpu_speed(); // in Mhz

int hardware_set_audio_output(int iAudioDevice, int iAudioVolume);

#ifdef __cplusplus
}  
#endif 