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

#include "../../base/base.h"
#include "../../base/config.h"
#include "menu.h"
#include "menu_diagnose_radio_link.h"
#include "../process_router_messages.h"

MenuDiagnoseRadioLink::MenuDiagnoseRadioLink(int iVehicleRadioLinkIndex)
:Menu(MENU_ID_DIAGNOSE_RADIO_LINK, "Diagnose Radio Link", NULL)
{
   m_iVehicleRadioLinkIndex = iVehicleRadioLinkIndex;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.4;

   m_bFailedToGetInfo = false;
   addTopLine("Getting radio interface info from vehicle.");
   addTopLine("Please wait...");

   addMenuItem(new MenuItem("Cancel"));
   m_bWaitingForData = true;
   memset(m_uDataFromVehicle, 0, 1024);
   memset(m_uDataFromController, 0, 1024);

   t_packet_header PH;
   u8 uLinkId = (u8)m_iVehicleRadioLinkIndex;
   u8 uCommandId = 1;
   
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
   memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
   for( int i=0; i<5; i++ )
      send_packet_to_router(buffer, PH.total_length);

   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
   
   uCommandId = 0;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
   memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
   send_packet_to_router(buffer, PH.total_length);
}


MenuDiagnoseRadioLink::~MenuDiagnoseRadioLink()
{
}


void MenuDiagnoseRadioLink::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(yTop);
}

void MenuDiagnoseRadioLink::onReceivedVehicleData(u8* pData, int iDataLength)
{
   log_line("MenuDiagnoseRadioLink: Got vehicle data, %d bytes: [%s]", iDataLength, (char*) pData);

   memcpy(m_uDataFromVehicle, pData, iDataLength);

   if ( (NULL != strstr((char*)m_uDataFromVehicle, "Invalid")) || (NULL != strstr((char*)m_uDataFromVehicle, "Failed")) )
   {
      m_bWaitingForData = false;
   
      removeAllItems();
      removeAllTopLines();
      addTopLine((char*)m_uDataFromVehicle);
      addMenuItem(new MenuItem("Done"));

      t_packet_header PH;
      u8 uLinkId = (u8)m_iVehicleRadioLinkIndex;
      u8 uCommandId = 2;
      
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
      PH.vehicle_id_src = g_uControllerId;
      PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
      PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
      
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
      memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
      send_packet_to_router(buffer, PH.total_length);
      return;
   }

   addTopLine("Getting radio interface info from controller.");
   addTopLine("Please wait...");

   t_packet_header PH;
   u8 uLinkId = (u8)m_iVehicleRadioLinkIndex;
   u8 uCommandId = 1;

   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
   memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
   send_packet_to_router(buffer, PH.total_length);

   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
   
   uCommandId = 0;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
   memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
   send_packet_to_router(buffer, PH.total_length);
}


void MenuDiagnoseRadioLink::onReceivedControllerData(u8* pData, int iDataLength)
{
   log_line("MenuDiagnoseRadioLink: Got controller data, %d bytes: [%s]", iDataLength, (char*) pData);
   memcpy(m_uDataFromController, pData, iDataLength);

   m_bWaitingForData = false;

   removeAllItems();
   removeAllTopLines();

   t_packet_header PH;
   u8 uLinkId = (u8)m_iVehicleRadioLinkIndex;
   u8 uCommandId = 2;
   
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
   memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
   for( int i=0; i<5; i++ )
      send_packet_to_router(buffer, PH.total_length);

   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header)+2*sizeof(u8);
   
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&uLinkId, sizeof(u8));
   memcpy(buffer+sizeof(t_packet_header) + sizeof(u8), (u8*)&uCommandId, sizeof(u8));
   send_packet_to_router(buffer, PH.total_length);

   if ( (NULL != strstr((char*)m_uDataFromController, "Invalid")) || (NULL != strstr((char*)m_uDataFromController, "Failed")) )
   {
      addTopLine((char*)m_uDataFromController);
      addMenuItem(new MenuItem("Done"));
      return;
   }

   char szParamC[32];
   char szParamV[32];

   int iPosV = 0;
   int iPosC = 0;

   addTopLine("Controller / Vehicle Params:");

   while ( (m_uDataFromController[iPosC] != 0) && (m_uDataFromVehicle[iPosV] != 0) )
   {
      szParamC[0] = 0;
      while ( (m_uDataFromController[iPosC] != 0) && (m_uDataFromController[iPosC] != '\n') )
      {
         szParamC[strlen(szParamC)] = m_uDataFromController[iPosC];
         iPosC++;
      }
      szParamC[strlen(szParamC)] = 0;
      if ( m_uDataFromController[iPosC] != '\n' )
         iPosC++;

      szParamV[0] = 0;
      while ( (m_uDataFromVehicle[iPosV] != 0) && (m_uDataFromVehicle[iPosV] != '\n') )
      {
         szParamV[strlen(szParamV)] = m_uDataFromVehicle[iPosV];
         iPosV++;
      }
      szParamV[strlen(szParamV)] = 0;
      if ( m_uDataFromVehicle[iPosV] != '\n' )
         iPosV++;
      char szBuff[64];
      sprintf(szBuff, "%s / %s", szParamC, szParamV);
      addTopLine(szBuff);
   }

   addMenuItem(new MenuItem("Done"));
}

void MenuDiagnoseRadioLink::onSelectItem()
{
   //if ( m_bWaitingForData )
   if ( 0 == m_SelectedIndex )
      menu_stack_pop(0);
}