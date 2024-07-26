#include "base.h"
#include "config.h"
#include "hardware_radio_txpower.h"
#include "hardware_radio.h"
#include "hw_procs.h"

void hardware_radio_set_txpower_rtl8812au(int iTxPower)
{
   log_line("Setting RTL8812AU TX Power to: %d", iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   #ifdef HW_PLATFORM_RASPBERRY

   int iHasOrgFile = 0;
   if ( access( "/etc/modprobe.d/rtl8812au.conf.org", R_OK ) != -1 )
      iHasOrgFile = 1;
   if ( iHasOrgFile )
   {
      FILE *pF = fopen("/etc/modprobe.d/rtl8812au.conf.org", "rb");
      if ( NULL == pF )
         iHasOrgFile = 0;
      else
      {
         fseek(pF, 0, SEEK_END);
         long fsize = ftell(pF);
         fclose(pF);
         if ( fsize < 20 || fsize > 512 )
            iHasOrgFile = 0;
      }
   }

   char szBuff[256];
   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl8812au.conf.org tmp/rtl8812au.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl8812au.conf; cp tmp/rtl8812au.conf /etc/modprobe.d/", iTxPower);
   else if ( access( "/etc/modprobe.d/rtl8812au.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl8812au.conf tmp/rtl8812au.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl8812au.conf; cp tmp/rtl8812au.conf /etc/modprobe.d/", iTxPower);
   else
      log_softerror_and_alarm("Can't access/find RTL power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);

   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl88XXau.conf.org tmp/rtl88XXau.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl88XXau.conf; cp tmp/rtl88XXau.conf /etc/modprobe.d/", iTxPower);
   else if ( access( "/etc/modprobe.d/rtl88XXau.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl88XXau.conf tmp/rtl88XXau.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl88XXau.conf; cp tmp/rtl88XXau.conf /etc/modprobe.d/", iTxPower);
   else
      log_softerror_and_alarm("Can't access/find RTL-XX power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);
   #endif

   #if defined(HW_PLATFORM_OPENIPC_CAMERA) || defined(HW_PLATFORM_OPENIPC_CAMERA)

   log_line("Set tx power now using iw dev...");
   char szComm[256];
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( (pRadioHWInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU) ||
            (pRadioHWInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU) ||
            (pRadioHWInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812AU) ||
            (pRadioHWInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL88X2BU) ||
            (pRadioHWInfo->iRadioDriver == RADIO_HW_DRIVER_MEDIATEK) )
      {
         sprintf(szComm, "iw dev %s set txpower fixed %d", pRadioHWInfo->szName, -100*iTxPower);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   #endif
   log_line("RTL8812AU TX Power changed to: %d", iTxPower);
}

void hardware_radio_set_txpower_rtl8812eu(int iTxPower)
{
   log_line("Setting RTL8812EU TX Power to: %d", iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   log_line("Set tx power now using iw dev...");
   char szComm[256];
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
      {
         sprintf(szComm, "iw dev %s set txpower fixed %d", pRadioHWInfo->szName, iTxPower*40);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   log_line("RTL8812EU TX Power changed to: %d", iTxPower);
}

void hardware_radio_set_txpower_atheros(int iTxPower)
{
   log_line("Setting Atheros TX Power to: %d", iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   #ifdef HW_PLATFORM_RASPBERRY
   int iHasOrgFile = 0;
   if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) != -1 )
      iHasOrgFile = 1;
   if ( iHasOrgFile )
   {
      FILE *pF = fopen("/etc/modprobe.d/ath9k_hw.conf.org", "rb");
      if ( NULL == pF )
         iHasOrgFile = 0;
      else
      {
         fseek(pF, 0, SEEK_END);
         long fsize = ftell(pF);
         fclose(pF);
         if ( fsize < 20 || fsize > 512 )
            iHasOrgFile = 0;
      }
   }

   char szBuff[256];
   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf.org tmp/ath9k_hw.conf; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", iTxPower);
   else if ( access( "/etc/modprobe.d/ath9k_hw.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf tmp/ath9k_hw.conf; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", iTxPower);
   else
      log_softerror_and_alarm("Can't access/find Atheros power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);

   // Change power for RALINK cards too. They are 2.4Ghz only cards 
   if ( access( "/etc/modprobe.d/rt2800usb.conf", R_OK ) != -1 )
   {
      iTxPower = (iTxPower/10)-2;
      if ( iTxPower < 0 ) iTxPower = 0;
      if ( iTxPower > 5 ) iTxPower = 5;
      sprintf(szBuff, "cp /etc/modprobe.d/rt2800usb.conf tmp/; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/rt2800usb.conf; cp tmp/rt2800usb.conf /etc/modprobe.d/", iTxPower);
      hw_execute_bash_command(szBuff, NULL);
   }
   #endif
   log_line("Atheros TX Power changed to: %d", iTxPower);
}
