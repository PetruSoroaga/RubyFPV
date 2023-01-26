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

#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "base.h"
#include "hardware_serial.h"
#include "config.h"
#include "hw_procs.h"
#include "../common/string_utils.h"

int s_OptionsSerialBaudRatesC[] = { 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };

int s_iHardwareSerialPortsWasInitialized = 0;

int s_iCountHardwareSerialPorts = 0;
hw_serial_port_info_t s_HardwareSerialPortsInfo[MAX_SERIAL_PORTS];

int* hardware_get_serial_baud_rates()
{
   return s_OptionsSerialBaudRatesC;
}

int hardware_get_serial_baud_rates_count()
{
   return sizeof(s_OptionsSerialBaudRatesC) / sizeof(s_OptionsSerialBaudRatesC[0]);
}

void _hardware_enumerate_serial_ports()
{
   s_iCountHardwareSerialPorts = 0;

   strcpy(s_HardwareSerialPortsInfo[0].szName, "Serial-0");
   strcpy(s_HardwareSerialPortsInfo[0].szPortDeviceName, "/dev/serial0");
   s_HardwareSerialPortsInfo[0].iSupported = 1;
   s_HardwareSerialPortsInfo[0].lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
   s_HardwareSerialPortsInfo[0].iPortUsage = SERIAL_PORT_USAGE_NONE;
   s_iCountHardwareSerialPorts = 1;

   for( int i=0; i<MAX_SERIAL_PORTS-4; i++ )
   {
      char szBuff[128];
      char szOutput[1024];
      sprintf(szBuff, "/dev/ttyUSB%d", i);
      if( access( szBuff, R_OK ) == -1 )
         continue;
      if ( s_iCountHardwareSerialPorts >= MAX_SERIAL_PORTS )
         continue;

      sprintf(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szName, "Serial-USB%d",i);
      strcpy(s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].szPortDeviceName, szBuff);
      s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 1;
      sprintf(szBuff, "ls /dev/serial/by-id/ -al | grep ttyUSB%d", i);
      hw_execute_bash_command_raw_silent(szBuff, szOutput);
   
      if ( NULL != strstr(szOutput, "CP2102") )
         s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 1;
      else if ( NULL != strstr(szOutput, "HL-340") )
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 0;
      else
      {
         hw_execute_bash_command_raw_silent("lsusb | grep Serial | grep 340 | grep USB", szOutput);
         if ( NULL != strstr(szOutput, "340") )
            s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iSupported = 0;
      }

      s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
      s_HardwareSerialPortsInfo[s_iCountHardwareSerialPorts].iPortUsage = SERIAL_PORT_USAGE_NONE;
      s_iCountHardwareSerialPorts++;
   }
}

int hardware_init_serial_ports()
{
   if ( s_iHardwareSerialPortsWasInitialized )
      return s_iCountHardwareSerialPorts;

   log_line("[Hardware] Initializing serial ports...");

   int iCountLoadedSerialPorts = 0;
   hw_serial_port_info_t loadedSerialPortsInfo[MAX_SERIAL_PORTS];

   FILE* fd = fopen(FILE_CONFIG_HW_SERIAL_PORTS, "r");
   if ( NULL != fd )
   {
      int iFailed = 0;
      if ( 1 != fscanf(fd, "%d", &iCountLoadedSerialPorts) )
         iFailed = 1;
      if ( iCountLoadedSerialPorts < 0 || iCountLoadedSerialPorts > MAX_SERIAL_PORTS )
      {
         iCountLoadedSerialPorts = 0;
         iFailed = 2;
      }
      for( int i=0; i<iCountLoadedSerialPorts; i++ )
      {
         if ( (!iFailed) && (1 != fscanf(fd, "%s", loadedSerialPortsInfo[i].szName)) )
            iFailed = 5;
         if ( (!iFailed) && (1 != fscanf(fd, "%s", loadedSerialPortsInfo[i].szPortDeviceName)) )
            iFailed = 3;
         for( int k=0; k<MAX_SERIAL_PORT_NAME; k++ )
            if ( loadedSerialPortsInfo[i].szName[k] == '*' )
               loadedSerialPortsInfo[i].szName[k] = ' ';
         for( int k=0; k<MAX_SERIAL_PORT_NAME; k++ )
            if ( loadedSerialPortsInfo[i].szPortDeviceName[k] == '*' )
               loadedSerialPortsInfo[i].szPortDeviceName[k] = ' ';

         loadedSerialPortsInfo[i].szName[MAX_SERIAL_PORT_NAME-1] = 0;
         loadedSerialPortsInfo[i].szPortDeviceName[MAX_SERIAL_PORT_NAME-1] = 0;
         if ( (!iFailed) && (3 != fscanf(fd, "%d %ld %d", &(loadedSerialPortsInfo[i].iSupported), &(loadedSerialPortsInfo[i].lPortSpeed),&(loadedSerialPortsInfo[i].iPortUsage))) )
            iFailed = 4;
      }
      if ( iFailed )
         iCountLoadedSerialPorts = 0;
      fclose(fd);
   }

   _hardware_enumerate_serial_ports();

   // Copy settings from the loaded serial ports to the detected serial ports

   int iCountMatched = 0;
   int iCountModified = 0;
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
   {
      for( int k=0; k<iCountLoadedSerialPorts; k++ )
         if ( 0 == strcmp(s_HardwareSerialPortsInfo[i].szPortDeviceName, loadedSerialPortsInfo[k].szPortDeviceName) )
         {
            iCountMatched++;
            strcpy(s_HardwareSerialPortsInfo[i].szName, loadedSerialPortsInfo[k].szName);
            if ( s_HardwareSerialPortsInfo[i].lPortSpeed != loadedSerialPortsInfo[k].lPortSpeed ) 
               iCountModified++;
            if ( s_HardwareSerialPortsInfo[i].iPortUsage != loadedSerialPortsInfo[k].iPortUsage )
               iCountModified++;
            s_HardwareSerialPortsInfo[i].lPortSpeed = loadedSerialPortsInfo[k].lPortSpeed;
            s_HardwareSerialPortsInfo[i].iPortUsage = loadedSerialPortsInfo[k].iPortUsage;
            break;
         }
   }
   s_iHardwareSerialPortsWasInitialized = 1;

   if ( (s_iCountHardwareSerialPorts != iCountLoadedSerialPorts) ||
        (iCountMatched != s_iCountHardwareSerialPorts) ||
        (iCountModified > 0) )
   {
      log_line("[Hardware] Serial Ports have changed. Saving current configuration.");
      hardware_serial_save_configuration();
   }

   log_line("[Hardware] Initialized %d serial ports:", s_iCountHardwareSerialPorts);
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
      log_line("[Hardware] Serial port %d: [%s] [%s], speed: %ld bps, usage: %d, supported: %d", i+1,
         s_HardwareSerialPortsInfo[i].szName,
         s_HardwareSerialPortsInfo[i].szPortDeviceName, s_HardwareSerialPortsInfo[i].lPortSpeed,
         s_HardwareSerialPortsInfo[i].iPortUsage,
         s_HardwareSerialPortsInfo[i].iSupported);

   return s_iCountHardwareSerialPorts;
}

void hardware_reload_serial_ports()
{
   FILE* fd = fopen(FILE_CONFIG_HW_SERIAL_PORTS, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Hardware] Failed to reload serial ports from config file.");
      return;

   }
   
   int iFailed = 0;
   if ( 1 != fscanf(fd, "%d", &s_iCountHardwareSerialPorts) )
      iFailed = 1;
   if ( s_iCountHardwareSerialPorts < 0 || s_iCountHardwareSerialPorts > MAX_SERIAL_PORTS )
   {
      s_iCountHardwareSerialPorts = 0;
      iFailed = 2;
   }
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
   {
      if ( (!iFailed) && (1 != fscanf(fd, "%s", s_HardwareSerialPortsInfo[i].szName)) )
         iFailed = 5;
      if ( (!iFailed) && (1 != fscanf(fd, "%s", s_HardwareSerialPortsInfo[i].szPortDeviceName)) )
         iFailed = 3;
      for( int k=0; k<MAX_SERIAL_PORT_NAME; k++ )
         if ( s_HardwareSerialPortsInfo[i].szName[k] == '*' )
            s_HardwareSerialPortsInfo[i].szName[k] = ' ';
      for( int k=0; k<MAX_SERIAL_PORT_NAME; k++ )
         if ( s_HardwareSerialPortsInfo[i].szPortDeviceName[k] == '*' )
            s_HardwareSerialPortsInfo[i].szPortDeviceName[k] = ' ';

      s_HardwareSerialPortsInfo[i].szName[MAX_SERIAL_PORT_NAME-1] = 0;
      s_HardwareSerialPortsInfo[i].szPortDeviceName[MAX_SERIAL_PORT_NAME-1] = 0;
      if ( (!iFailed) && (3 != fscanf(fd, "%d %ld %d", &(s_HardwareSerialPortsInfo[i].iSupported), &(s_HardwareSerialPortsInfo[i].lPortSpeed),&(s_HardwareSerialPortsInfo[i].iPortUsage))) )
         iFailed = 4;
   }
   if ( iFailed )
      s_iCountHardwareSerialPorts = 0;
   fclose(fd);

   log_line("[Hardware] Reloaded %d serial ports:", s_iCountHardwareSerialPorts);
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
      log_line("[Hardware] Serial port %d: [%s] [%s], speed: %ld bps, usage: %d, supported: %d", i+1,
         s_HardwareSerialPortsInfo[i].szName,
         s_HardwareSerialPortsInfo[i].szPortDeviceName, s_HardwareSerialPortsInfo[i].lPortSpeed,
         s_HardwareSerialPortsInfo[i].iPortUsage,
         s_HardwareSerialPortsInfo[i].iSupported);
}

void hardware_serial_save_configuration()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      return;

   FILE* fd = fopen(FILE_CONFIG_HW_SERIAL_PORTS, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Hardware] Failed to save hardware serial ports configuration.");
      return;
   }

   fprintf(fd, "%d\n", s_iCountHardwareSerialPorts);
   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
   {
      char szBuff[MAX_SERIAL_PORT_NAME+1];
      strcpy(szBuff, s_HardwareSerialPortsInfo[i].szName);
      for( int k=0; k<strlen(szBuff); k++ )
         if ( szBuff[k] == ' ' )
           szBuff[k] = '*';
      fprintf(fd, "%s\n", szBuff);
      
      strcpy(szBuff, s_HardwareSerialPortsInfo[i].szPortDeviceName);
      for( int k=0; k<strlen(szBuff); k++ )
         if ( szBuff[k] == ' ' )
           szBuff[k] = '*';
      fprintf(fd, "%s\n", szBuff);
      fprintf(fd, "%d %ld %d\n", s_HardwareSerialPortsInfo[i].iSupported, s_HardwareSerialPortsInfo[i].lPortSpeed, s_HardwareSerialPortsInfo[i].iPortUsage );
   }
   fclose(fd);
}

int hardware_has_unsupported_serial_ports()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_init_serial_ports();

   for( int i=0; i<s_iCountHardwareSerialPorts; i++ )
      if ( s_HardwareSerialPortsInfo[i].iSupported == 0 )
         return 1;
   return 0;
}

int hardware_get_serial_ports_count()
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_init_serial_ports();
   return s_iCountHardwareSerialPorts;
}

hw_serial_port_info_t* hardware_get_serial_port_info(int iPortIndex)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_init_serial_ports();

   if ( iPortIndex >= 0 && iPortIndex < s_iCountHardwareSerialPorts )
      return &(s_HardwareSerialPortsInfo[iPortIndex]);
   return NULL;
}


int hardware_configure_serial(const char* szDevName, long baudRate)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_init_serial_ports();
   return 1;
   //char szBuff[1024];
   //sprintf(szBuff, "stty -F %s -icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon %ld", szDevName, baudRate);
   //execute_bash_command(szBuff, NULL);
   
   int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NDELAY );
   if ( -1 == fPort )
   {
      log_softerror_and_alarm("Failed to open serial port %s", szDevName);
      return 0;
   }
   
   struct termios options;
   tcgetattr(fPort, &options);
   cfmakeraw(&options);

   switch(baudRate)
   {
      case 2400:  cfsetospeed(&options, B2400); break;
      case 4800:  cfsetospeed(&options, B4800); break;
      case 9600:  cfsetospeed(&options, B9600); break;
      case 19200: cfsetospeed(&options, B19200); break;
      case 38400: cfsetospeed(&options, B38400); break;
      case 57600: cfsetospeed(&options, B57600); break;
      case 115200: cfsetospeed(&options, B115200); break;
      default:    cfsetospeed(&options, B57600); break;
   }

   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   options.c_lflag &= ~ECHO; // no echo
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   options.c_cflag |= CLOCAL; // Set local mode on
   
   options.c_iflag = IGNBRK;
   options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   options.c_iflag |= IXANY;
   //options.c_cc[VMIN] = 0;
   //options.c_cc[VTIME] = 5;
   
   tcsetattr(fPort, TCSANOW, &options); //write options 
   close(fPort);
   

   log_line("Configured serial port %s to baudrate %ld bps.", szDevName, baudRate);
   return 1;

}

int hardware_open_serial_port(const char* szDevName, long baudRate)
{
   if ( ! s_iHardwareSerialPortsWasInitialized )
      hardware_init_serial_ports();
   if ( NULL == szDevName || 0 == szDevName[0] )
   {
      log_softerror_and_alarm("Hardware: Invalid serial port name to open.");
      return -1;
   }

   //int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NONBLOCK );
   int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NDELAY );
   //int fPort = open ("/dev/serial0", O_RDONLY | O_NOCTTY | O_NDELAY );

   // working:
   //int fPort = open (szDevName, O_RDWR | O_NOCTTY | O_NDELAY );

   if ( -1 == fPort )
   {
      log_softerror_and_alarm("Failed to open serial port %s", szDevName);
      return -1;
   }
   
   struct termios options;
   tcgetattr(fPort, &options);
   cfmakeraw(&options);

   switch(baudRate)
   {
      case 2400:  cfsetospeed(&options, B2400); break;
      case 4800:  cfsetospeed(&options, B4800); break;
      case 9600:  cfsetospeed(&options, B9600); break;
      case 19200: cfsetospeed(&options, B19200); break;
      case 38400: cfsetospeed(&options, B38400); break;
      case 57600: cfsetospeed(&options, B57600); break;
      case 115200: cfsetospeed(&options, B115200); break;
      default:    cfsetospeed(&options, B57600); break;
   }

   /*
   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   options.c_cflag |= CREAD | CLOCAL; // Set local mode on
   
   options.c_lflag &= ~ECHO; // no echo
   options.c_lflag &= ~ECHOE; // no echo
   options.c_lflag &= ~ECHONL; // no echo
   options.c_lflag &= ~ICANON;
   options.c_lflag &= ~ISIG;

   options.c_iflag = IGNBRK;
   options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   options.c_iflag |= IXANY;
   options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

   options.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
   options.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed 
   */

   options.c_cflag &= ~CSIZE;
   options.c_cflag |= CS8; // Set 8 data bits
   options.c_cflag &= ~PARENB; // Set no parity
   options.c_cflag &= ~CSTOPB; // 1 stop bit
   options.c_lflag &= ~ECHO; // no echo
   options.c_cflag &= ~CRTSCTS; // no RTS/CTS Flow Control
   options.c_cflag |= CLOCAL; // Set local mode on
   
   options.c_iflag = IGNBRK;
   options.c_iflag &= ~(IXON | IXOFF ); // No software handshake
   options.c_iflag |= IXANY;

   //options.c_cc[VMIN] = 0;
   //options.c_cc[VTIME] = 5;

   tcsetattr(fPort, TCSANOW, &options); //write options 

   log_line("[Hardware]: Opened serial port %s with baudrate %ld bps.", szDevName, baudRate);

   return fPort;
}

