#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"

#include <time.h>
#include <sys/resource.h>
#include<stdio.h>
#include<sys/socket.h> 


bool bQuit = false;

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
} 
  
int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestSocketIn");
   log_enable_stdout();
   log_line("\nStarted.\n");

   int port = 5600;


   if ( argc >= 2 )
      port = atoi(argv[1]);

   int socket_server, socket_client;
   struct sockaddr_in server_addr, client_addr;
	
   socket_server = socket(AF_INET , SOCK_DGRAM, 0);
   if (socket_server == -1)
   {
      log_line("Can't create socket.");
      exit(-1);
   }

   memset(&server_addr, 0, sizeof(server_addr));
   memset(&client_addr, 0, sizeof(client_addr));
    
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = INADDR_ANY;
   server_addr.sin_port = htons( port );
	
   if( bind(socket_server,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_line("Failed to bind socket.");
      close(socket_server);
      exit(-1);
   }

   log_line("Waiting for clients on port %d ...", port);

   while (!bQuit)
   {
      char szBuff[1025];
      socklen_t len = sizeof(client_addr);
      int nRecv = recvfrom(socket_server, szBuff, 1024, 
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
      if ( nRecv > 0 )
      {
         szBuff[nRecv] = '\0';
         log_line("Received from client: [%s]", szBuff);
      }
      else
         log_line("Failed to receive from client.");
   }

   /*
   listen(socket_desc , 5);

   log_line("Socket created and binded on port: %d", port);
   log_line("Waiting for clients.");
   while (!bQuit)
   {
      int c = sizeof(struct sockaddr_in);
      socket_client = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
      if (socket_client<0)
         log_line("Failed to connect client socket.");
      log_line("Client connected.");
      while( !bQuit )
      {
         char szBuff[1025];
         int read_size = recv(socket_client, szBuff, 1024 , 0);
         if ( read_size > 0 )
         {
             szBuff[read_size] = 0;
             log_line("Recv: [%s]", szBuff);
         }

      }
   }

   */
   close(socket_server);
   log_line("\nEnded\n");
   exit(0);
}
