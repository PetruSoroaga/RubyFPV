#include "hdmi.h"
#include "base.h"
#include "hardware.h"
#include "hw_procs.h"


#define MAX_HDMI_RESOLUTIONS 20
#define MAX_RESOLUTION_REFRESH_RATES_COUNT 10
#define HDMI_ASPECT_MODE_4_3 0
#define HDMI_ASPECT_MODE_16_9 1
#define HDMI_ASPECT_MODE_16_10 2
#define HDMI_ASPECT_MODE_21_9 3


int s_nHDMI_CurrentResolutionIndex = -1;
int s_nHDMI_CurrentResolutionRefreshIndex = -1;
int s_nHDMI_ResolutionCount = 0;
int s_nHDMI_ResolutionWidth[MAX_HDMI_RESOLUTIONS];
int s_nHDMI_ResolutionHeight[MAX_HDMI_RESOLUTIONS];
int s_nHDMI_ResolutionRefreshCount[MAX_HDMI_RESOLUTIONS];
int s_nHDMI_ResolutionRefresh[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];
int s_nHDMI_ResolutionGroup[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];
int s_nHDMI_ResolutionMode[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];
int s_nHDMI_ResolutionAspect[MAX_HDMI_RESOLUTIONS][MAX_RESOLUTION_REFRESH_RATES_COUNT];

void _hdmi_add_resolution(int group, int mode, int w, int h, int refresh_rate, int aspect)
{
   if ( s_nHDMI_ResolutionCount >= MAX_HDMI_RESOLUTIONS )
      return;

   int index = -1;

   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      if ( s_nHDMI_ResolutionWidth[i] == w && s_nHDMI_ResolutionHeight[i] == h )
      {
         index = i;
         break;
      }
   }

   if ( index == -1 )
   {
      index = s_nHDMI_ResolutionCount;
      s_nHDMI_ResolutionWidth[index] = w;
      s_nHDMI_ResolutionHeight[index] = h;
      s_nHDMI_ResolutionRefreshCount[index] = 0;
      s_nHDMI_ResolutionCount++;
   }

   int indexR = -1;

   for( int i=0; i<s_nHDMI_ResolutionRefreshCount[index]; i++ )
   {
      if ( s_nHDMI_ResolutionRefresh[index][i] == refresh_rate )
      {
         indexR = i;
         break;
      }
   }

   if ( indexR != -1 )
      return;
   if ( s_nHDMI_ResolutionRefreshCount[index] >= MAX_RESOLUTION_REFRESH_RATES_COUNT )
      return;

   indexR = s_nHDMI_ResolutionRefreshCount[index];
   s_nHDMI_ResolutionRefreshCount[index]++;

   s_nHDMI_ResolutionRefresh[index][indexR] = refresh_rate;
   s_nHDMI_ResolutionGroup[index][indexR] = group;
   s_nHDMI_ResolutionMode[index][indexR] = mode;
   s_nHDMI_ResolutionAspect[index][indexR] = aspect;
}

void _hdmi_detect_current_mode()
{
   log_line("Detecting current HDMI mode...");

   char szBuff[1024];
   int hgroup = 0;
   int hmode = 0;
   szBuff[0] = 0;
   hw_execute_bash_command_silent("cat /boot/config.txt | grep hdmi_group | sed -r 's/[=]+//g' | sed -r 's/[hdmi_group]+//g'", szBuff);
   if ( 0 != szBuff[0] )
      sscanf(szBuff, "%d", &hgroup);

   szBuff[0] = 0;   
   hw_execute_bash_command_silent("cat /boot/config.txt | grep hdmi_mode | sed -r 's/[=]+//g' | sed -r 's/[hdmi_mode]+//g'", szBuff);
   if ( 0 != szBuff[0] )
      sscanf(szBuff, "%d", &hmode);

   if ( hgroup <= 0 || hmode <= 0 )
   {
      log_softerror_and_alarm("Failed to detect current HDMI mode (g:%d, m:%d)", hgroup, hmode);
      return;
   }
   if ( hgroup != 1 && hgroup != 2 )
   {
      log_softerror_and_alarm("Failed to detect current HDMI mode. (g:%d, m:%d)", hgroup, hmode);
      return;
   }

   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      for( int k=0; k<s_nHDMI_ResolutionRefreshCount[i]; k++ )
      {
         if ( s_nHDMI_ResolutionGroup[i][k] == hgroup )
         if ( s_nHDMI_ResolutionMode[i][k] == hmode )
         {
            s_nHDMI_CurrentResolutionIndex = i;
            s_nHDMI_CurrentResolutionRefreshIndex = k;
            log_line("Current HDMI mode: (g:%d, m:%d), width, height: %d x %d, refresh: %d Hz", hgroup, hmode, s_nHDMI_ResolutionWidth[s_nHDMI_CurrentResolutionIndex], s_nHDMI_ResolutionHeight[s_nHDMI_CurrentResolutionIndex], s_nHDMI_ResolutionRefresh[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex]);
            return;
         }
      }
   }
   log_softerror_and_alarm("Failed to detect current HDMI mode. (g:%d, m:%d)", hgroup, hmode);
}

void hdmi_init_modes()
{
   s_nHDMI_ResolutionCount = 0;
   s_nHDMI_CurrentResolutionIndex = -1;
   s_nHDMI_CurrentResolutionRefreshIndex = -1;

   _hdmi_add_resolution(1,1, 640, 480, 60, HDMI_ASPECT_MODE_4_3);
   _hdmi_add_resolution(2,9, 800, 600, 60, HDMI_ASPECT_MODE_4_3);
   _hdmi_add_resolution(2,16, 1024, 768, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(1,61, 1280, 720, 25, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,62, 1280, 720, 30, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,19, 1280, 720, 50, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,4,  1280, 720, 60, HDMI_ASPECT_MODE_16_9);

   _hdmi_add_resolution(2,28,  1280, 800, 60, HDMI_ASPECT_MODE_16_10);

   _hdmi_add_resolution(2,32,  1280, 960, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(2,35,  1280, 1024, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(2,51,  1600, 1200, 60, HDMI_ASPECT_MODE_4_3);

   _hdmi_add_resolution(1,33,  1920, 1080, 25, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,34,  1920, 1080, 30, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(1,31,  1920, 1080, 50, HDMI_ASPECT_MODE_16_9);

   _hdmi_add_resolution(2,69,  1920, 1200, 60, HDMI_ASPECT_MODE_16_9);
   _hdmi_add_resolution(2,70,  1920, 1200, 75, HDMI_ASPECT_MODE_16_9);
}

int hdmi_get_resolutions_count()
{
   return s_nHDMI_ResolutionCount;
}

int hdmi_get_current_resolution_index()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   return s_nHDMI_CurrentResolutionIndex;
}

int hdmi_get_current_resolution_group()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionGroup[0][0];

   return s_nHDMI_ResolutionGroup[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex];
}

int hdmi_get_current_resolution_mode()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionMode[0][0];

   return s_nHDMI_ResolutionMode[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex];
}


int hdmi_get_current_resolution_width()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionWidth[0];

   return s_nHDMI_ResolutionWidth[s_nHDMI_CurrentResolutionIndex];
}

int hdmi_get_current_resolution_height()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();

   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionHeight[0];

   return s_nHDMI_ResolutionHeight[s_nHDMI_CurrentResolutionIndex];
}

int hdmi_get_current_resolution_refresh()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionRefresh[0][0];

   return s_nHDMI_ResolutionRefresh[s_nHDMI_CurrentResolutionIndex][s_nHDMI_CurrentResolutionRefreshIndex];
}

int hdmi_get_current_resolution_refresh_count()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return s_nHDMI_ResolutionRefreshCount[0];

   return s_nHDMI_ResolutionRefreshCount[s_nHDMI_CurrentResolutionIndex];
}

int hdmi_get_current_resolution_refresh_index()
{
   if ( -1 == s_nHDMI_CurrentResolutionIndex )
      _hdmi_detect_current_mode();
   if ( -1 == s_nHDMI_CurrentResolutionIndex || -1 == s_nHDMI_CurrentResolutionRefreshIndex )
      return 0;
   return s_nHDMI_CurrentResolutionRefreshIndex;
}


int hdmi_get_resolution_width(int index)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionWidth[0];
   return s_nHDMI_ResolutionWidth[index];
}

int hdmi_get_resolution_height(int index)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionHeight[0];
   return s_nHDMI_ResolutionHeight[index];
}

int hdmi_get_resolution_refresh_count(int index)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionRefreshCount[0];
   return s_nHDMI_ResolutionRefreshCount[index];
}

int hdmi_get_resolution_refresh_rate(int index, int index2)
{
   if ( index < 0 || index >= s_nHDMI_ResolutionCount )
      return s_nHDMI_ResolutionRefresh[0][0];
   if ( index2 < 0 || index2 >= s_nHDMI_ResolutionRefreshCount[index] )
      return s_nHDMI_ResolutionRefresh[index][0];
   return s_nHDMI_ResolutionRefresh[index][index2];
}

int hdmi_set_current_resolution(int width, int height, int refresh)
{
   char szBuff[256];
   int index = -1;
   int indexR = -1;
   for( int i=0; i<s_nHDMI_ResolutionCount; i++ )
   {
      for( int k=0; k<s_nHDMI_ResolutionRefreshCount[i]; k++ )
      {
         if ( s_nHDMI_ResolutionWidth[i] == width )
         if ( s_nHDMI_ResolutionHeight[i] == height )
         if ( s_nHDMI_ResolutionRefresh[i][k] == refresh )
         {
            index = i;
            indexR = k;
            break;
         }
      }
   }
   if ( index == -1 || indexR == -1 )
   {
      log_softerror_and_alarm("Failed to change HDMI mode. Invalid mode: %d x %d, refresh: %d Hz", width, height, refresh);
      return -1;
   }

   log_line("Changed HDMI mode to: (g:%d, m:%d): %d x %d, refresh: %d Hz", s_nHDMI_ResolutionGroup[index][indexR], s_nHDMI_ResolutionMode[index][indexR], width, height, refresh);

   s_nHDMI_CurrentResolutionIndex = index;
   s_nHDMI_CurrentResolutionRefreshIndex = indexR;

   hardware_mount_boot();
   hardware_sleep_ms(50);
   hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

   snprintf(szBuff, sizeof(szBuff), "sed -i 's/hdmi_group=[0-9]*/hdmi_group=%d/g' config.txt", s_nHDMI_ResolutionGroup[index][indexR]);
   hw_execute_bash_command(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "sed -i 's/hdmi_mode=[0-9]*/hdmi_mode=%d/g' config.txt", s_nHDMI_ResolutionMode[index][indexR]);
   hw_execute_bash_command(szBuff, NULL);
   hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);

   return 0;
}
