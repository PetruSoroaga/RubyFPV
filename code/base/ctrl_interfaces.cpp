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
#include "hardware.h"
#include "hw_procs.h"
#include "hardware_radio.h"
#include "ctrl_interfaces.h"
#include "ctrl_settings.h"
#include "../common/string_utils.h"

ControllerInterfacesSettings s_CIS;
bool s_bAddedNewRadioInterfaces = false;

bool controllerAddedNewRadioInterfaces()
{
   return s_bAddedNewRadioInterfaces;
}

void _controller_interfaces_add_card(const char* szMAC)
{
   if ( NULL == szMAC || 0 == szMAC[0] )
      return;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info_from_mac(szMAC);

   strlcpy(s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szMAC, szMAC, MAX_MAC_LENGTH);
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].cardModel = 0;
   if ( NULL != pRadioInfo )
      s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].cardModel = pRadioInfo->iCardModel;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].capabilities_flags = RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;

   if ( hardware_radio_is_sik_radio(pRadioInfo) )
      s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].capabilities_flags &= ~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);

   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].datarate = 0;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szUserDefinedName = (char*)malloc(2);
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szUserDefinedName[0] = 0;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].iInternal = false;
   s_CIS.radioInterfacesCount++;
   s_bAddedNewRadioInterfaces = true;
}

int _controller_interfaces_get_card_index(const char* szMAC)
{
   if ( NULL == szMAC || 0 == szMAC[0] )
      return -1;
   int index = -1;
   for( int i=0; i<s_CIS.radioInterfacesCount; i++ )
      if ( 0 == strncmp(szMAC, s_CIS.listRadioInterfaces[i].szMAC, MAX_MAC_LENGTH) )
      {
         index = i;
         break;
      }
   if ( index == -1 )
   if ( s_CIS.radioInterfacesCount < CONTROLLER_INTERFACES_MAX_LIST_SIZE-1 )
   {
      index = s_CIS.radioInterfacesCount;
      _controller_interfaces_add_card(szMAC);
   }
   return index;
}

void reset_ControllerInterfacesSettings()
{
   memset(&s_CIS, 0, sizeof(s_CIS));
   s_CIS.radioInterfacesCount = 0;
   s_CIS.listMACTXPreferredCount = 0;
   s_CIS.inputInterfacesCount = 0;
}

int save_ControllerInterfacesSettings()
{
   FILE* fd = fopen(FILE_CONTROLLER_INTERFACES, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save controller interfaces settings to file: %s",FILE_CONTROLLER_INTERFACES);
      return 0;
   }

   fprintf(fd, "%s\n", CONTROLLER_INTERFACES_STAMP_ID);
   
   fprintf(fd, "radio_cards_settings: %d\n", s_CIS.radioInterfacesCount );
   for( int i=0; i<s_CIS.radioInterfacesCount; i++ )
   {
      char szBuff[1024];
      strcpy(szBuff, s_CIS.listRadioInterfaces[i].szUserDefinedName);
      int kSize = (int)strlen(szBuff);
      for( int k=0; k<kSize; k++ )
         if ( szBuff[k] == ' ' || szBuff[k] == 10 || szBuff[k] == 13 )
            szBuff[k] = '~';
      if ( szBuff[0] == '~' && szBuff[1] == 0 )
         szBuff[0] = 0;
      if (  0 == szBuff[0] )
         fprintf(fd, "~ %s %d %u %d\n", s_CIS.listRadioInterfaces[i].szMAC, s_CIS.listRadioInterfaces[i].cardModel, s_CIS.listRadioInterfaces[i].capabilities_flags, s_CIS.listRadioInterfaces[i].datarate);
      else
         fprintf(fd, "%s %s %d %u %d\n", szBuff, s_CIS.listRadioInterfaces[i].szMAC, s_CIS.listRadioInterfaces[i].cardModel, s_CIS.listRadioInterfaces[i].capabilities_flags, s_CIS.listRadioInterfaces[i].datarate);
      fprintf(fd, "%d\n", s_CIS.listRadioInterfaces[i].iInternal);
   }

   fprintf(fd, "TX_Preferred: %d\n", s_CIS.listMACTXPreferredCount );
   for( int i=0; i<s_CIS.listMACTXPreferredCount; i++ )
      fprintf(fd, " %s\n", s_CIS.listMACTXPreferredOrdered[i]);

   fprintf(fd, "Input_Interfaces: %d\n", s_CIS.inputInterfacesCount );
   for( int i=0; i<s_CIS.inputInterfacesCount; i++ )
   {
      char szName[MAX_JOYSTICK_INTERFACE_NAME];
      strcpy(szName, s_CIS.inputInterfaces[i].szInterfaceName );
      int len = strlen(szName);
      for( int j=0; j<len; j++ )
         if ( szName[j] == ' ' )
            szName[j] = '~';
      fprintf(fd, "%d %s\n", i+1, szName );
      fprintf(fd, "%u %d %d %d\n", s_CIS.inputInterfaces[i].uId, s_CIS.inputInterfaces[i].bCalibrated, s_CIS.inputInterfaces[i].countAxes, s_CIS.inputInterfaces[i].countButtons );
      for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         fprintf(fd, "%d %d %d %d\n", s_CIS.inputInterfaces[i].axesMinValue[k], s_CIS.inputInterfaces[i].axesMaxValue[k], s_CIS.inputInterfaces[i].axesCenterValue[k], s_CIS.inputInterfaces[i].axesCenterZone[k] );

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         fprintf(fd, "%d ", s_CIS.inputInterfaces[i].buttonsReleasedValue[k] );
      fprintf(fd, "\n");

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         fprintf(fd, "%d ", s_CIS.inputInterfaces[i].buttonsMinValue[k] );
      fprintf(fd, "\n");

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         fprintf(fd, "%d ", s_CIS.inputInterfaces[i].buttonsMaxValue[k] );
      fprintf(fd, "\n");
   }

   // Extra params

   if ( NULL != fd )
      fclose(fd);

   log_line("Saved controller interfaces settings to file: %s", FILE_CONTROLLER_INTERFACES);
   log_line("Saved controller interfaces settings for %d radio interfaces, %d input devices.", s_CIS.radioInterfacesCount, s_CIS.inputInterfacesCount);
   return 1;
}

int load_ControllerInterfacesSettings()
{
   FILE* fd = fopen(FILE_CONTROLLER_INTERFACES, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (missing file). Reset and create the file.",FILE_CONTROLLER_INTERFACES);
      reset_ControllerInterfacesSettings();
      save_ControllerInterfacesSettings();
      return 0;
   }

   int failed = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, CONTROLLER_INTERFACES_STAMP_ID) )
      failed = 2;

   if ( failed )
   {
      reset_ControllerInterfacesSettings();
      log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid config file stamp)",FILE_CONTROLLER_INTERFACES);
      fclose(fd);
      return 0;
   }

   // Radio interfaces

   s_CIS.radioInterfacesCount = 0;
   if ( 1 != fscanf(fd, "%*s %d", &s_CIS.radioInterfacesCount) )
   {
      failed = 4;
      s_CIS.radioInterfacesCount = 0;
      log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid ri count)",FILE_CONTROLLER_INTERFACES);
   }

   if ( ! failed )
   for( int i=0; i<s_CIS.radioInterfacesCount; i++ )
   {
      u32 tmp = 0;
      int tmp2 = 0;
      s_CIS.listRadioInterfaces[i].szUserDefinedName = (char*)malloc(64);
      if ( 5 != fscanf(fd, "%s %s %d %u %d", s_CIS.listRadioInterfaces[i].szUserDefinedName, s_CIS.listRadioInterfaces[i].szMAC, &(s_CIS.listRadioInterfaces[i].cardModel), &tmp, &tmp2) )
      {
         failed = 5;
         log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid ri data)",FILE_CONTROLLER_INTERFACES);
      }
      if ( failed )
      {
         s_CIS.radioInterfacesCount = 0;
         break;
      }

      if ( 1 != fscanf(fd, "%d", &s_CIS.listRadioInterfaces[i].iInternal) )
      {
         failed = 1;
         s_CIS.radioInterfacesCount = 0;
         break;
      }

      s_CIS.listRadioInterfaces[i].capabilities_flags = tmp;
      s_CIS.listRadioInterfaces[i].datarate = tmp2;

      int kSize = (int)strlen(s_CIS.listRadioInterfaces[i].szUserDefinedName);
      for( int k=0; k<kSize; k++ )
         if ( s_CIS.listRadioInterfaces[i].szUserDefinedName[k] == '~' )
            s_CIS.listRadioInterfaces[i].szUserDefinedName[k] = ' ';

      if ( 0 == s_CIS.listRadioInterfaces[i].szUserDefinedName[0] || (s_CIS.listRadioInterfaces[i].szUserDefinedName[0] == ' ' && 0 == s_CIS.listRadioInterfaces[i].szUserDefinedName[1]) )
      {
          s_CIS.listRadioInterfaces[i].szUserDefinedName[0] = 0;
      }
   }

   s_CIS.listMACTXPreferredCount = 0;
   if ( 1 != fscanf(fd, "%*s %d", &s_CIS.listMACTXPreferredCount) )
   {
      failed = 6;
      s_CIS.listMACTXPreferredCount = 0;
      log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid txp count)",FILE_CONTROLLER_INTERFACES);
   }

   if ( ! failed )
   for( int i=0; i<s_CIS.listMACTXPreferredCount; i++ )
   {
      if ( 1 != fscanf(fd, "%s", s_CIS.listMACTXPreferredOrdered[i]) )
      {
         failed = 7;
         log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid txp data)",FILE_CONTROLLER_INTERFACES);
      }
      if ( failed )
      {
         s_CIS.listMACTXPreferredCount = 0;
         break;
      }
   }

   // Input interfaces

   if ( (!failed) && (1 != fscanf(fd, "%*s %d", &s_CIS.inputInterfacesCount)) )
   {
      failed = 8;
      s_CIS.inputInterfacesCount = 0;
      log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid input icount)",FILE_CONTROLLER_INTERFACES);
   }
   if ( ! failed )
   for( int i=0; i<s_CIS.inputInterfacesCount; i++ )
   {
      int index = 0;
      if ( 2 != fscanf(fd, "%d %s", &index, s_CIS.inputInterfaces[i].szInterfaceName) )
      {
         failed = 9;
         log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid ii data)",FILE_CONTROLLER_INTERFACES);
      }
      if ( index != i+1 )
         failed = 10;
      if ( failed == 1 )
         break;
      int len = strlen(s_CIS.inputInterfaces[i].szInterfaceName);
      for( int j=0; j<len; j++ )
         if ( s_CIS.inputInterfaces[i].szInterfaceName[j] == '~' )
            s_CIS.inputInterfaces[i].szInterfaceName[j] = ' ';
      int tmp1, tmp2, tmp3;
      if ( 4 != fscanf(fd, "%u %d %d %d", &s_CIS.inputInterfaces[i].uId, &tmp1, &tmp2, &tmp3) )
         failed = 11;
      s_CIS.inputInterfaces[i].bCalibrated = tmp1;
      s_CIS.inputInterfaces[i].countAxes = tmp2;
      s_CIS.inputInterfaces[i].countButtons = tmp3;
      s_CIS.inputInterfaces[i].currentHardwareIndex = -1;

      for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         if ( 4 != fscanf(fd, "%d %d %d %d", &s_CIS.inputInterfaces[i].axesMinValue[k], &s_CIS.inputInterfaces[i].axesMaxValue[k], &s_CIS.inputInterfaces[i].axesCenterValue[k], &s_CIS.inputInterfaces[i].axesCenterZone[k]) )
            failed = 12;

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         if ( 1 != fscanf(fd, "%d", &s_CIS.inputInterfaces[i].buttonsReleasedValue[k]) )
            failed = 13;

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         if ( 1 != fscanf(fd, "%d", &s_CIS.inputInterfaces[i].buttonsMinValue[k]) )
            failed = 14;

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         if ( 1 != fscanf(fd, "%d", &s_CIS.inputInterfaces[i].buttonsMaxValue[k]) )
            failed = 15;
   }

   // Extra params
   if ( ! failed )
   {
   }

   if ( failed )
   {
      log_softerror_and_alarm("Failed to load controller interfaces settings from file: %s (invalid config file, error code: %d)",FILE_CONTROLLER_INTERFACES, failed);
      fclose(fd);
      return 0;
   }
   fclose(fd);
   log_line("Loaded controller interfaces settings from file: %s", FILE_CONTROLLER_INTERFACES);
   log_line("Loaded controller interfaces settings for %d radio interfaces, %d input devices.", s_CIS.radioInterfacesCount, s_CIS.inputInterfacesCount);
   return 1;
}

ControllerInterfacesSettings* get_ControllerInterfacesSettings()
{
   return &s_CIS;
}

void controllerRadioInterfacesLogInfo()
{
   char szBuff[1024];

   log_line("=================================================================");
   log_line("Controller radio interfaces flags and info:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioInfo->szMAC);
      szBuff[0] = 0;
      if ( controllerIsCardRXOnly(pRadioInfo->szMAC) )
         strlcat(szBuff, "[RX ONLY]", sizeof(szBuff));
      if ( controllerIsCardTXOnly(pRadioInfo->szMAC) )
         strlcat(szBuff, "[TX ONLY]", sizeof(szBuff));
      if ( ! controllerIsCardRXOnly(pRadioInfo->szMAC) )
      if ( ! controllerIsCardTXOnly(pRadioInfo->szMAC) )
         strlcat(szBuff, "[TX/RX]", sizeof(szBuff));
      if ( NULL != pCardInfo )
         log_line("* RadioInterface %d: %s, %s MAC:%s phy#%d, %s %s", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), pRadioInfo->szName, pRadioInfo->szMAC, pRadioInfo->phy_index, (controllerIsCardDisabled(pRadioInfo->szMAC)?"[DISABLED]":"[ENABLED]"), szBuff);
      else
         log_line("* RadioInterface %d: %s, %s MAC:%s phy#%d, %s %s", i+1, "Unknown Type", pRadioInfo->szName, pRadioInfo->szMAC, pRadioInfo->phy_index, (controllerIsCardDisabled(pRadioInfo->szMAC)?"[DISABLED]":"[ENABLED]"), szBuff);
      u32 uFlags = controllerGetCardFlags(pRadioInfo->szMAC);
      int nRate = controllerGetCardDataRate(pRadioInfo->szMAC);
      szBuff[0] = 0;
      str_get_radio_capabilities_description(uFlags, szBuff);
      log_line("      Controller set rate: %d, set flags: %s", nRate, szBuff);
   }
   log_line("=================================================================");
}

t_ControllerRadioInterfaceInfo* controllerGetRadioCardInfo(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return NULL;
   return &(s_CIS.listRadioInterfaces[index]);
}

int controllerIsCardDisabled(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 1;

   if ( s_CIS.listRadioInterfaces[index].capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )   
      return 1;
   return 0;
}

int controllerIsCardRXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 1;
   if ( !(s_CIS.listRadioInterfaces[index].capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      return 1;
   return 0;
}

int controllerIsCardTXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 1;
   if ( !(s_CIS.listRadioInterfaces[index].capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      return 1;
   return 0;
}

void controllerSetCardDisabled(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_DISABLED;

   log_line("Controller interfaces: Added radio interface %s to disabled list", szMAC);
   save_ControllerInterfacesSettings();
}

void controllerRemoveCardDisabled(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = s_CIS.listRadioInterfaces[index].capabilities_flags & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);

   log_line("Controller interfaces: Removed radio interface %s from disabled list", szMAC);
   save_ControllerInterfacesSettings();
}


void controllerSetCardTXRX(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;

   log_line("Controller interfaces: Set radio interface %s as TX/RX", szMAC);

   save_ControllerInterfacesSettings();
}

void controllerSetCardRXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = (s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_CAN_RX) & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX);

   log_line("Controller interfaces: Set radio interface %s as RX only", szMAC);

   save_ControllerInterfacesSettings();
}

void controllerSetCardTXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = (s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_CAN_TX) & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX);

   log_line("Controller interfaces: Set radio interface %s as TX only", szMAC);

   save_ControllerInterfacesSettings();
}


// Returns the priority index (lower = higher priority )
int controllerIsCardTXPreferred(const char* szMAC)
{
   if ( NULL == szMAC || 0 == szMAC[0] )
      return 0;
   for( int i=0; i<s_CIS.listMACTXPreferredCount; i++ )
      if ( 0 == strcmp(szMAC, s_CIS.listMACTXPreferredOrdered[i]) )
         return i+1;
   return 0;
}

int controllerGetTXPreferredIndexForCard(const char* szMAC)
{
   return controllerIsCardTXPreferred(szMAC);
}

void controllerSetCardTXPreferred(const char* szMAC)
{
   if ( NULL == szMAC || 0 == szMAC[0] )
      return;

   // Is already on top of the list?
   int iCurrentPriority = controllerIsCardTXPreferred(szMAC);
   if ( 1 == iCurrentPriority )
      return;

   // Is already in the list? Move it to first position
   if ( iCurrentPriority > 0 )
   {
      char szTmp[MAX_MAC_LENGTH+1];
      strcpy(szTmp, s_CIS.listMACTXPreferredOrdered[iCurrentPriority-1]);

      for( int i=iCurrentPriority-1; i>0; i-- )
         strcpy(s_CIS.listMACTXPreferredOrdered[i], s_CIS.listMACTXPreferredOrdered[i-1]);

      strcpy(s_CIS.listMACTXPreferredOrdered[0], szTmp);
      save_ControllerInterfacesSettings();
      return;
   }

   if ( s_CIS.listMACTXPreferredCount >= CONTROLLER_INTERFACES_MAX_LIST_SIZE )
      return;

   log_line("Adding radio interface %s to preffered TX list on top.", szMAC);

   for( int i=s_CIS.listMACTXPreferredCount-1; i>0; i--)
      strcpy(s_CIS.listMACTXPreferredOrdered[i], s_CIS.listMACTXPreferredOrdered[i-1]);
   strcpy(s_CIS.listMACTXPreferredOrdered[0], szMAC);
   s_CIS.listMACTXPreferredCount++;
   
   save_ControllerInterfacesSettings();
}

void controllerRemoveCardTXPreferred(const char* szMAC)
{
   if ( NULL == szMAC || 0 == szMAC[0] )
      return;
   int iCurrentPriority = controllerIsCardTXPreferred(szMAC);
   if ( iCurrentPriority <= 0 )
      return;

   for( int i=iCurrentPriority-1; i<s_CIS.listMACTXPreferredCount-1; i++ )
      strcpy(s_CIS.listMACTXPreferredOrdered[i], s_CIS.listMACTXPreferredOrdered[i+1]);

   s_CIS.listMACTXPreferredCount--;
   
   log_line("Removed radio interface %s from preffered TX list.", szMAC);
   save_ControllerInterfacesSettings();
}

void controllerSetCardInternal(const char* szMAC, bool bInternal)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;
   s_CIS.listRadioInterfaces[index].iInternal = (int)bInternal;
}

bool controllerIsCardInternal(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return false;
   return (bool) s_CIS.listRadioInterfaces[index].iInternal;
}

u32 controllerGetCardFlags(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 0;

   return s_CIS.listRadioInterfaces[index].capabilities_flags;
}

void controllerSetCardFlags(const char* szMAC, u32 flags)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = flags;
}

int controllerGetCardDataRate(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 0;
   return s_CIS.listRadioInterfaces[index].datarate;
}

void controllerSetCardDataRate(const char* szMAC, int dataRate)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;
   s_CIS.listRadioInterfaces[index].datarate = dataRate;
}

char* controllerGetCardUserDefinedName(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return NULL;
   return s_CIS.listRadioInterfaces[index].szUserDefinedName;
}

void controllerSetCardUserDefinedName(const char* szMAC, const char* szName)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   if ( NULL != s_CIS.listRadioInterfaces[index].szUserDefinedName )
      free(s_CIS.listRadioInterfaces[index].szUserDefinedName);

   if ( NULL == szName || 0 == szName[0] || (' ' == szName[0] && 0 == szName[1])  )
   {
      s_CIS.listRadioInterfaces[index].szUserDefinedName = (char*) malloc(2);
      s_CIS.listRadioInterfaces[index].szUserDefinedName[0] = 0;
      return;
   }
   s_CIS.listRadioInterfaces[index].szUserDefinedName = (char*) malloc(strlen(szName)+1);
   strcpy(s_CIS.listRadioInterfaces[index].szUserDefinedName, szName);
}

/*
int _controllerComputeRXTXCardsSingleFrequency(Model* pModel, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX)
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }

      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }

      *pFrequencies = pModel->radioLinksParams.link_frequency[0];
      pFrequencies++;
   }

   int* pListTXCards = pTXCardIndexes;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
         continue;
      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         continue;

      if ( 0 == hardware_radioindex_supports_frequency(i, pModel->radioLinksParams.link_frequency[0]) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         *pRXCardIndexes = i;
         pRXCardIndexes++;
         countCardsRX++;
         log_line("Added card %d: %s to RX list", i+1, pRadio->szUSBPort);
      }

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         *pTXCardIndexes = i;
         pTXCardIndexes++;
         countCardsTX++;
         log_line("Added card %d: %s to TX list", i+1, pRadio->szUSBPort);
      }
   }

   // Leave only one tx card in the list, the one that is most preffered;
   // If automatic selection, leave all of them (tx) in the list

   ControllerSettings* pCS = get_ControllerSettings();

   if ( 0 == pCS->nAutomaticTxCard )
   {
      int maxPrefferedValue = -1;
      int maxPrefferedIndex = -1;
      radio_hw_info_t* pRadioTx = NULL;
      for( int i=0; i<countCardsTX; i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(pListTXCards[i]);
         int prefValue = controllerGetTXPreferredIndexForCard(pRadio->szMAC);
         if ( prefValue >= maxPrefferedValue )
         {
            maxPrefferedValue = prefValue;
            maxPrefferedIndex = pListTXCards[i];
            pRadioTx = pRadio;
         }
      }

      if ( maxPrefferedIndex >= 0 )
      {
         pListTXCards[0] = maxPrefferedIndex;
         log_line("Setting card %d: %s as the one used for TX.", maxPrefferedIndex, pRadioTx->szUSBPort);
      }
      countCardsTX = 1;
   }
   log_line("Total cards used for vehicle radio links: %d cards will RX, %d cards will TX", countCardsRX, countCardsTX);

   if ( 0 == countCardsRX || 0 == countCardsTX )
   {
      log_softerror_and_alarm("Failed to find a Tx or Rx card for the vehicle link.");
      return 0;
   }
   return 1;
}

int _controllerComputeRXTXCardsDualFrequency(Model* pModel, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX)
{
   // First, detect which cards on the controller support which vehicle radio links frequencies

   bool bNicSupportsRxMain[MAX_RADIO_INTERFACES];
   bool bNicSupportsRxSecondary[MAX_RADIO_INTERFACES];
   bool bNicSupportsTxMain[MAX_RADIO_INTERFACES];
   bool bNicSupportsTxSecondary[MAX_RADIO_INTERFACES];

   int iCountSupportsRxMain = 0;
   int iCountSupportsRxSecondary = 0;
   int iCountSupportsTxMain = 0;
   int iCountSupportsTxSecondary = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      bNicSupportsRxMain[i] = false;
      bNicSupportsRxSecondary[i] = false;
      bNicSupportsTxMain[i] = false;
      bNicSupportsTxSecondary[i] = false;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
         continue;
      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         continue;

      if ( hardware_radio_supports_frequency(pRadio, pModel->radioLinksParams.link_frequency[0]) )
      {
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsRxMain[i] = true;
            iCountSupportsRxMain++;
         }
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsTxMain[i] = true;
            iCountSupportsTxMain++;
         }
      }
      if ( hardware_radio_supports_frequency(pRadio, pModel->radioLinksParams.link_frequency[1]) )
      {
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsRxSecondary[i] = true;
            iCountSupportsRxSecondary++;
         }
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsTxSecondary[i] = true;
            iCountSupportsTxSecondary++;
         }
      }
      log_line("Card %d supports: RX main: %s, TX main: %s, RX secondary: %s, TX secondary: %s", i, bNicSupportsRxMain[i]?"yes":"no", bNicSupportsTxMain[i]?"yes":"no", bNicSupportsRxSecondary[i]?"yes":"no", bNicSupportsTxSecondary[i]?"yes":"no");
   }
   log_line("Total: %d cards supports RX main, %d cards supports TX main, %d cards supports RX secondary, %d cards supports TX secondary.", iCountSupportsRxMain, iCountSupportsTxMain, iCountSupportsRxSecondary, iCountSupportsTxSecondary);

   int countAllocatedForMain = 0;
   int countAllocatedForSecondary = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }
      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }

      if ( ( 1 == iCountSupportsRxMain && bNicSupportsRxMain[i] ) ||
           ( 1 == iCountSupportsTxMain && bNicSupportsTxMain[i] ) )
      {
         *pFrequencies = pModel->radioLinksParams.link_frequency[0];
         pFrequencies++;
         countAllocatedForMain++;
      }
      else if ( ( 1 == iCountSupportsRxSecondary && bNicSupportsRxSecondary[i] ) ||
           ( 1 == iCountSupportsTxSecondary && bNicSupportsTxSecondary[i] ) )
      {
         *pFrequencies = pModel->radioLinksParams.link_frequency[1];
         pFrequencies++;
         countAllocatedForSecondary++;
      }
      else if ( bNicSupportsRxMain[i] || bNicSupportsTxMain[i] )
      {
         if ( countAllocatedForMain > 0 )
         if ( countAllocatedForSecondary == 0 )
         if ( bNicSupportsRxSecondary[i] || bNicSupportsTxSecondary[i] )
         {
            *pFrequencies = pModel->radioLinksParams.link_frequency[1];
            pFrequencies++;
            countAllocatedForSecondary++;
            continue;
         }
         *pFrequencies = pModel->radioLinksParams.link_frequency[0];
         pFrequencies++;
         countAllocatedForMain++;
      }
      else
      {
         *pFrequencies = pModel->radioLinksParams.link_frequency[1];
         pFrequencies++;
         countAllocatedForSecondary++;
      }
   }

   bool bSetTxForMain = false;
   bool bSetTxForSecondary = false;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
         continue;

      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      {
         *pRXCardIndexes = i;
         pRXCardIndexes++;
         countCardsRX++;
      }
      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      {
         if ( 1 == iCountSupportsTxMain && bNicSupportsTxMain[i] && (!bSetTxForMain) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForMain = true;
         }
         else if ( 1 == iCountSupportsTxSecondary && bNicSupportsTxSecondary[i] && (!bSetTxForSecondary) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForSecondary = true;
         }
         else if ( bNicSupportsTxSecondary[i] && (!bSetTxForSecondary) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForSecondary = true;
         }
         else if ( ! bSetTxForMain )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForMain = true;
         }
      }
   }

   log_line("Total cards used for main frequency: %d cards will RX, %d cards will TX", iCountSupportsRxMain, iCountSupportsTxMain);
   log_line("Total card used for secondary frequency: %d will RX, %d cards will TX", iCountSupportsRxSecondary, iCountSupportsTxSecondary);

   if ( (! bSetTxForMain) || (! bSetTxForSecondary) )
   {
      log_softerror_and_alarm("Failed to find a Tx card for main or secondary vehicle link.");
      return 0;
   }
   if ( iCountSupportsRxMain == 0 || iCountSupportsRxSecondary == 0 ||
        iCountSupportsTxMain == 0 || iCountSupportsTxSecondary == 0 )
   {
      log_softerror_and_alarm("Failed to find a Tx or Rx card for main or secondary vehicle link.");
      return 0;
   }
   return 1;
}

int controllerComputeRXTXCards(Model* pModel, int iSearchFreq, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX)
{
   // If model is null, it's default model connect wait
   // If search freq is not 0, it's searching for models on a frequency

   if ( NULL == pFrequencies || NULL == pRXCardIndexes || NULL == pTXCardIndexes )
   {
      log_line("ControllerComputeRXTXCards: Invalid params or default vehicle.");
      return 0;
   }

   countCardsRX = 0;
   countCardsTX = 0;
   int* pListTXCards = pTXCardIndexes;

   // Is searching for vehicle on a frequency ?

   if ( 0 < iSearchFreq )
   {
      log_line("ControllerComputeRXTXCards: Searching mode: use fixed frequency for all and default params.");
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(i);
         if ( NULL == pRadio )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }
         u32 flags = controllerGetCardFlags(pRadio->szMAC);
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }

         *pFrequencies = iSearchFreq;
         pFrequencies++;

         if ( 0 == hardware_radio_supports_frequency(pRadio, iSearchFreq ) )
            continue;

         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) && (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            *pRXCardIndexes = i;
            pRXCardIndexes++;
            countCardsRX++;
         }
      }
      if ( 0 == countCardsRX )
         return 0;
      return 1;
   }

   // First pairing

   if ( NULL == pModel )
   {
      log_line("ControllerComputeRXTXCards: First pairing, use default frequencies and params.");
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(i);
         if ( NULL == pRadio )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }
         u32 flags = controllerGetCardFlags(pRadio->szMAC);
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }

         int freq = DEFAULT_FREQUENCY;
         if ( pRadio->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
            freq = DEFAULT_FREQUENCY58;

         *pFrequencies = freq;
         pFrequencies++;

         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            *pRXCardIndexes = i;
            pRXCardIndexes++;
            countCardsRX++;
         }
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
         }
      }

      // Leave only one tx card in the list, the one that is most preffered;
      int maxPrefferedValue = -1;
      int maxPrefferedIndex = -1;
      for( int i=0; i<countCardsTX; i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(pListTXCards[i]);
         int prefValue = controllerGetTXPreferredIndexForCard(pRadio->szMAC);
         if ( prefValue >= maxPrefferedValue )
         {
            maxPrefferedValue = prefValue;
            maxPrefferedIndex = i;
         }
      }
      if ( maxPrefferedIndex >= 0 )
         pListTXCards[0] = maxPrefferedIndex;

      countCardsTX = 1;
      if ( 0 == countCardsRX || 0 == countCardsTX )
         return 0;
      return 1;
   }


   // Model with a single radio link

   if ( 1 == pModel->radioInterfacesParams.interfaces_count || 0 == pModel->radioLinksParams.link_frequency[1] )
   {
      log_line("ControllerComputeRXTXCards: Detected single radio link vehicle.");         
      return _controllerComputeRXTXCardsSingleFrequency(pModel, pFrequencies, pRXCardIndexes, countCardsRX, pTXCardIndexes, countCardsTX);
      
   }

   // Model with multiple links

   log_line("ControllerComputeRXTXCards: Detected single radio link vehicle.");         
   return _controllerComputeRXTXCardsDualFrequency(pModel, pFrequencies, pRXCardIndexes, countCardsRX, pTXCardIndexes, countCardsTX);
}
*/



void controllerInterfacesEnumJoysticks()
{
   bool bNewInterfacesDetected = false;

   hardware_enum_joystick_interfaces();
   for( int i=0; i<hardware_get_joystick_interfaces_count(); i++ )
   {
      hw_joystick_info_t* pJoystick = hardware_get_joystick_info(i);
      if ( NULL == pJoystick )
         continue;

      bool bFound = false;
      for( int j=0; j<s_CIS.inputInterfacesCount; j++ )
      {
         if ( 0 != strcmp(pJoystick->szName, s_CIS.inputInterfaces[j].szInterfaceName) )
            continue;
         if ( pJoystick->uId != s_CIS.inputInterfaces[j].uId )
            continue;
         if ( pJoystick->countAxes != s_CIS.inputInterfaces[j].countAxes )
            continue;
         if ( pJoystick->countButtons != s_CIS.inputInterfaces[j].countButtons )
            continue;
         bFound = true;
         s_CIS.inputInterfaces[j].currentHardwareIndex = i;
      }
      if ( ! bFound && s_CIS.inputInterfacesCount < CONTROLLER_MAX_INPUT_INTERFACES-1 )
      {
         bNewInterfacesDetected = true;
         strcpy(s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].szInterfaceName, pJoystick->szName);
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].uId = pJoystick->uId;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].countAxes = pJoystick->countAxes;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].countButtons = pJoystick->countButtons;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].bCalibrated = false;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].currentHardwareIndex = i;
         for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         {
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesCenterZone[k] = 10; // 1% center zone
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesCenterValue[k] = 0;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesMinValue[k] = -99;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesMaxValue[k] = 99;
         }
         for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         {
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].buttonsReleasedValue[k] = 0;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].buttonsMinValue[k] = 0;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].buttonsMaxValue[k] = 1;
         }
         s_CIS.inputInterfacesCount++;
      }
   }

   if ( bNewInterfacesDetected )
      save_ControllerInterfacesSettings();
}

t_ControllerInputInterface* controllerInterfacesGetAt(int index)
{
   if ( index < 0 || index >= hardware_get_joystick_interfaces_count() )
      return NULL;
   
   hw_joystick_info_t* pJoystick = hardware_get_joystick_info(index);
   if ( NULL == pJoystick )
      return NULL;

   for( int i=0; i<s_CIS.inputInterfacesCount; i++ )
   {
      if ( s_CIS.inputInterfaces[i].currentHardwareIndex == index )
         return &s_CIS.inputInterfaces[i];
   }
   return NULL;
}

