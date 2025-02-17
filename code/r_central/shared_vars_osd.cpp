#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_radio.h"
#include "shared_vars.h"
#include <math.h>

float g_fOSDDbm[MAX_RADIO_INTERFACES];
float g_fOSDSNR[MAX_RADIO_INTERFACES];


void shared_vars_osd_reset_before_pairing()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_fOSDDbm[i] = -200.0f;
      g_fOSDSNR[i] = 0.0;
   }
}

void shared_vars_osd_update()
{
   for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( ! pRadioHWInfo->isHighCapacityInterface )
         continue;

      int idbmMaxForRadioCard = -1000;
      int iSNRMaxForRadioCard = -1000;
      /*
      for( int k=0; k<g_SM_RadioStats.radio_interfaces[i].signalInfo.iAntennaCount; k++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].signalInfo.dbmValuesAll.iDbmMax[k] < 1000 )
         if ( g_SM_RadioStats.radio_interfaces[i].signalInfo.dbmValuesAll.iDbmMax[k] > idbmMaxForRadioCard )
            idbmMaxForRadioCard = g_SM_RadioStats.radio_interfaces[i].signalInfo.dbmValuesAll.iDbmMax[k];
      }
      */

      int iIndex = g_SMControllerRTInfo.iCurrentIndex;
      iIndex--;
      if ( iIndex < 0 )
         iIndex = SYSTEM_RT_INFO_INTERVALS-1;
      for( int iAnt=0; iAnt<g_SMControllerRTInfo.radioInterfacesDbm[iIndex][i].iCountAntennas; iAnt++ )
      {
         if ( g_SMControllerRTInfo.radioInterfacesDbm[iIndex][i].iDbmMax[iAnt] < 1000 )
         if ( g_SMControllerRTInfo.radioInterfacesDbm[iIndex][i].iDbmMax[iAnt] > idbmMaxForRadioCard )
         {
            idbmMaxForRadioCard = g_SMControllerRTInfo.radioInterfacesDbm[iIndex][i].iDbmMax[iAnt];

            int iSNR = g_SMControllerRTInfo.radioInterfacesDbm[iIndex][i].iDbmMax[iAnt] - g_SMControllerRTInfo.radioInterfacesDbm[iIndex][i].iDbmNoiseMin[iAnt];
            if ( iSNR < 1000 )
            if ( iSNR > idbmMaxForRadioCard )
               iSNRMaxForRadioCard = iSNR;
         }
      }
     
      if ( idbmMaxForRadioCard > -500 )
      {
         if ( fabs(g_fOSDDbm[i] - (float)idbmMaxForRadioCard) > 10.0 )
            g_fOSDDbm[i] = 0.6 * g_fOSDDbm[i] + 0.4 * (float)idbmMaxForRadioCard;
         else
            g_fOSDDbm[i] = 0.5 * g_fOSDDbm[i] + 0.5 * (float)idbmMaxForRadioCard;
      }
      if ( iSNRMaxForRadioCard > -500 )
      {
         if ( iSNRMaxForRadioCard < g_fOSDSNR[i] - 10 )
            g_fOSDSNR[i] = (float)iSNRMaxForRadioCard;
         else if ( iSNRMaxForRadioCard < g_fOSDSNR[i] - 5 )
            g_fOSDSNR[i] = 0.4 * g_fOSDSNR[i] + 0.6 * (float)iSNRMaxForRadioCard;
         if ( fabs(g_fOSDSNR[i] - (float)iSNRMaxForRadioCard) > 10.0 )
            g_fOSDSNR[i] = 0.6 * g_fOSDSNR[i] + 0.4 * (float)iSNRMaxForRadioCard;
         else
            g_fOSDSNR[i] = 0.5 * g_fOSDSNR[i] + 0.5 * (float)iSNRMaxForRadioCard;
      }
   }
}