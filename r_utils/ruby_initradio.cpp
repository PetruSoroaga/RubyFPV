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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../common/string_utils.h"

Model* g_pModelVehicle = NULL;

bool configure_nics_succeeded = false;

int init_NICs()
{
   char szBuff[1024];
   configure_nics_succeeded = false;

   log_line("=================================================================");
   log_line("Configuring NICs: START.");

   if( access( "tmp/ruby/conf_nics", R_OK ) != -1 )
   {
      log_line("NICs already configured. Do nothing.");
      log_line("=================================================================");
      configure_nics_succeeded = true;
      return 1;
   }
   
   u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   bool isStation = hardware_is_station();

   if ( isStation )
   {
      log_line("Configuring NICs: we are station.");
      load_Preferences();
      load_ControllerInterfacesSettings();

      Preferences* pP = get_Preferences();
      if ( NULL != pP )
         delayMs = (u32) pP->iDebugWiFiChangeDelay;
      if ( delayMs<1 || delayMs > 200 )
         delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   }
   else
   {
      log_line("Configuring NICs: we are vehicle.");
      reset_ControllerInterfacesSettings();
   }
   system("iw reg set DE");

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("No network cards found. Nothing to configure!");
      log_line("=================================================================");
      return 0;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
      log_line( "Configuring NIC: %s ...", nicInfo->szName );
      if ( ! nicInfo->isSupported )
      {
         log_softerror_and_alarm("Found unsupported NIC: %s, skipping.",nicInfo->szName);
         continue;
      }

      sprintf(szBuff, "ifconfig %s mtu 2304", nicInfo->szName );
      hw_execute_bash_command(szBuff, NULL);
      hardware_sleep_ms(delayMs);

      if ( ((nicInfo->typeAndDriver) & 0xFF) == RADIO_TYPE_ATHEROS )
      {
         //sprintf(szBuff, "ifconfig %s down", nicInfo->szName );
         //execute_bash_command(szBuff, NULL);
         //hardware_sleep_ms(delayMs);
         //sprintf(szBuff, "iw dev %s set type managed", nicInfo->szName );
         //execute_bash_command(szBuff, NULL);
         //hardware_sleep_ms(delayMs);

         sprintf(szBuff, "ifconfig %s up", nicInfo->szName );
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
         int dataRate = DEFAULT_DATARATE_VIDEO;
         if ( isStation )
            dataRate = controllerGetCardDataRate(nicInfo->szMAC);
         else if ( NULL != g_pModelVehicle )
         {
            if ( g_pModelVehicle->radioInterfacesParams.interface_link_id[i] >= 0 )
            if ( g_pModelVehicle->radioInterfacesParams.interface_link_id[i] < g_pModelVehicle->radioLinksParams.links_count )
               dataRate = g_pModelVehicle->radioLinksParams.link_datarates[g_pModelVehicle->radioInterfacesParams.interface_link_id[i]][0];
         }
         if ( dataRate == 0 )
            dataRate = DEFAULT_DATARATE_VIDEO;
         if ( dataRate > 0 )
            sprintf(szBuff, "iw dev %s set bitrates legacy-2.4 %d", nicInfo->szName, dataRate );
         else
            sprintf(szBuff, "iw dev %s set bitrates ht-mcs-2.4 %d", nicInfo->szName, -dataRate-1 );
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
         sprintf(szBuff, "ifconfig %s down", nicInfo->szName );
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
         sprintf(szBuff, "iw dev %s set monitor none", nicInfo->szName);
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
         sprintf(szBuff, "ifconfig %s up", nicInfo->szName );
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
         //printf(szBuff, "iw dev %s set monitor fcsfail", nicInfo->szName);
         //execute_bash_command(szBuff, NULL);
         //hardware_sleep_ms(delayMs);
      }
      //if ( ((nicInfo->type) & 0xFF) == RADIO_TYPE_RALINK )
      else
      {
         //sprintf(szBuff, "ifconfig %s down", nicInfo->szName );
         //execute_bash_command(szBuff, NULL);
         //hardware_sleep_ms(delayMs);

         sprintf(szBuff, "iw dev %s set monitor none", nicInfo->szName);
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
         sprintf(szBuff, "ifconfig %s up", nicInfo->szName );
         hw_execute_bash_command(szBuff, NULL);
         hardware_sleep_ms(delayMs);
      }

      if ( hardware_radioindex_supports_frequency(i, DEFAULT_FREQUENCY58) )
      {
         sprintf(szBuff, "iw dev %s set freq %d", nicInfo->szName, DEFAULT_FREQUENCY58);
         nicInfo->currentFrequency = DEFAULT_FREQUENCY58;
         hw_execute_bash_command(szBuff, NULL);
         nicInfo->lastFrequencySetFailed = 0;
         nicInfo->failedFrequency = 0;
      }
      else if ( hardware_radioindex_supports_frequency(i, DEFAULT_FREQUENCY) )
      {
         sprintf(szBuff, "iw dev %s set freq %d", nicInfo->szName, DEFAULT_FREQUENCY);
         nicInfo->currentFrequency = DEFAULT_FREQUENCY;
         hw_execute_bash_command(szBuff, NULL);
         nicInfo->lastFrequencySetFailed = 0;
         nicInfo->failedFrequency = 0;
      }
      else
      {
         nicInfo->currentFrequency = 0;
         nicInfo->lastFrequencySetFailed = 1;
         nicInfo->failedFrequency = DEFAULT_FREQUENCY;
      }
      //if ( isStation && controllerIsCardDisabled(nicInfo->szMAC) )
      //{
      //   sprintf(szBuff, "ifconfig %s down", nicInfo->szName );
      //   execute_bash_command(szBuff, NULL);
      //}
      hardware_sleep_ms(delayMs);
   }

   FILE* fd = fopen("tmp/ruby/conf_nics", "w");
   fprintf(fd, "done");
   fclose(fd);
   
   hardware_save_radio_info();
   log_line("Configuring NICs COMPLETED.");
   log_line("=================================================================");
   log_line("Radio interfaces and frequencies assigned:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      log_line("   * Radio interface %d: %s on port %s, %s %s %s: %d Mhz", i+1, str_get_radio_card_model_string(pNICInfo->iCardModel), pNICInfo->szUSBPort, pNICInfo->szName, pNICInfo->szMAC, pNICInfo->szDescription, pNICInfo->currentFrequency);
   }
   log_line("=================================================================");
   configure_nics_succeeded = true;
   return 1;
}


int main(int argc, char *argv[])
{
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }
   

   log_init("RubyRadioInit");

   hardware_enumerate_radio_interfaces();

   int retry = 0;
   while ( 0 == hardware_get_radio_interfaces_count() && retry < 10 )
   {
      hardware_sleep_ms(200);
      hardware_enumerate_radio_interfaces();
      retry++;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("There are no radio interfaces (NICS/wlans) on this device.");
      return -1;
   }    

   g_pModelVehicle = new Model();
   if ( ! g_pModelVehicle->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
   {
      log_softerror_and_alarm("Can't load current model vehicle.");
      g_pModelVehicle = NULL;
   }
   
   init_NICs();

   log_line("Ruby Init Radio process completed.");
   return (0);
} 