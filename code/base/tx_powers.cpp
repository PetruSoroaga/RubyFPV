/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "tx_powers.h"
#include <math.h>

/*
static int s_iTxBoosterGainTable4W[][2] =
{
   {2, 100},
   {5, 230},
   {10, 420},
   {20, 800},
   {30, 1100},
   {40, 1300},
   {50, 1400},
   {60, 1500},
   {70, 1600},
   {80, 1700},
   {100, 4000}
};
*/

static int s_iRawTxRadioValues[] = {1,5,10,15,20,23,26,30,35,40,45,50,53,56,60,63,65,68,70};

static int s_iTxUIPowerLevelsMw[] =
{ 1, 5, 10, 25, 50, 75, 100,
  150, 200, 250, 300, 350, 400, 450, 500, 600, 700, 800, 900, 1000};

static int s_iTxRawPowerLevelMeasurementsValues[] =
   { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------

static int s_iTxInfo722N[] =
   { 1,   1,   2,    3,   10,   25,   35,   60,   80,   90,    0,    0,    0};
static int s_iTxInfoBlueStick[]  =
   { 1,   2,   4,    8,   28,   50,   80,  110,  280, 1000,   0,     0,    0};
static int s_iTxInfoAWUS036NH[] =
   { 1,  10,  20,   30,   40,   50,   60,    0,    0,    0,    0,    0,    0};
static int s_iTxInfoAWUS036NHA[] =
   { 1,   1,   2,    6,   17,   70,  120,  180,  215,  310,  460,    0,    0};

// { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
static int s_iTxInfo58Generic[] =
   { 1,   5,  17,   50,  150,  170,  190,  210,  261,  310,    0,    0,    0};
static int s_iTxInfoAWUS036ACH[] =
   { 1,   2,   5,   20,   50,   90,  160,  250,  300,  420,  500,    0,    0};
static int s_iTxInfoASUSUSB56[]  =
   { 1,   4,  13,   42,   116, 190,  280,  360,  420,  490,  540,    0,    0}; // measured 08.dec.2024, ruby 10.1
static int s_iTxInfoRTLDualAnt[] =
   { 1,   1,   2,    4,   15,   25,   40,   45,   55,   70,   80,   80,    0}; // measured 23.dec.2024, ruby 10.1
static int s_iTxInfoAli1W[]  =
   { 1,   1,   2,    5,   10,   20,   30,   50,  100,  300,  450,    0,    0};
static int s_iTxInfoA6100[] =
   { 1,   1,   3,   10,   17,   19,   22,   23,   25,    0,    0,    0,    0}; // measured 08.dec.2024, ruby 10.1
static int s_iTxInfoAWUS036ACS[] =
   { 1,   1,   2,    3,   10,   25,   35,   50,   60,   90,  110,    0,    0};
static int s_iTxInfoArcherT2UP[] =
   { 1,   3,  10,   25,   55,   75,  110,  120,  140,  150,    0,    0,    0};

// { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
static int s_iTxInfoRTL8812EU[] =
   { 6,   7,  15,   45,  110,  160,  230,  270,  320,  380,  430,  500,  550}; // measured 08.dec.2024, ruby 10.1
   

// { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
static int s_iTxInfoOIPCUSight[] = 
 { 550, 600, 650,  670,  0,  0,  0,  0,  0,  0,  0,  0,  0}; // measured 23.dec.2024, ruby 10.1



bool _tx_powers_is_card_serial_radio(int iDriverType, int iCardModel)
{
   if ( (iDriverType == RADIO_HW_DRIVER_SERIAL_SIK) || (iDriverType == RADIO_HW_DRIVER_SERIAL) )
      return true;
   if ( (iCardModel == CARD_MODEL_SIK_RADIO) || (iCardModel == CARD_MODEL_SERIAL_RADIO) || (iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS) )
      return true;
   return false;
}

const int* _tx_powers_get_mw_table_for_card(int iDriverType, int iCardModel)
{
   int* piMwPowers = s_iTxInfo58Generic;
   switch (iCardModel)
   {
      case CARD_MODEL_TPLINK722N: piMwPowers = s_iTxInfo722N; break;
      case CARD_MODEL_ATHEROS_GENERIC: piMwPowers = s_iTxInfo722N; break;
      case CARD_MODEL_BLUE_STICK: piMwPowers = s_iTxInfoBlueStick; break;
      case CARD_MODEL_ALFA_AWUS036NH: piMwPowers = s_iTxInfoAWUS036NH; break;
      case CARD_MODEL_ALFA_AWUS036NHA: piMwPowers = s_iTxInfoAWUS036NHA; break;

      case CARD_MODEL_RTL8812AU_GENERIC: piMwPowers = s_iTxInfoRTLDualAnt; break;
      case CARD_MODEL_RTL8812AU_DUAL_ANTENNA: piMwPowers = s_iTxInfoRTLDualAnt; break;
      case CARD_MODEL_ASUS_AC56: piMwPowers = s_iTxInfoASUSUSB56; break;
      case CARD_MODEL_ALFA_AWUS036ACH: piMwPowers = s_iTxInfoAWUS036ACH; break;
      case CARD_MODEL_ALFA_AWUS036ACS: piMwPowers = s_iTxInfoAWUS036ACS; break;
      case CARD_MODEL_ZIPRAY: piMwPowers = s_iTxInfoAli1W; break;
      case CARD_MODEL_NETGEAR_A6100: piMwPowers = s_iTxInfoA6100; break;
      case CARD_MODEL_TENDA_U12: piMwPowers = s_iTxInfo58Generic; break;
      case CARD_MODEL_ARCHER_T2UPLUS: piMwPowers = s_iTxInfoArcherT2UP; break;
      case CARD_MODEL_RTL8814AU: piMwPowers = s_iTxInfo58Generic; break;
      case CARD_MODEL_RTL8812AU_OIPC_USIGHT: piMwPowers = s_iTxInfoOIPCUSight; break;

      case CARD_MODEL_BLUE_8812EU: piMwPowers = s_iTxInfoRTL8812EU; break;
   }
   return piMwPowers;
}

const int* tx_powers_get_raw_radio_power_values(int* piOutputCount)
{
   if ( NULL != piOutputCount )
      *piOutputCount = sizeof(s_iRawTxRadioValues)/sizeof(s_iRawTxRadioValues[0]);
   return s_iRawTxRadioValues;
}

const int* tx_powers_get_ui_levels_mw(int* piOutputCount)
{
   if ( NULL != piOutputCount )
      *piOutputCount = sizeof(s_iTxUIPowerLevelsMw)/sizeof(s_iTxUIPowerLevelsMw[0]);
   return s_iTxUIPowerLevelsMw;
}

const int* tx_powers_get_raw_measurement_intervals(int* piOutputCount)
{
   if ( NULL != piOutputCount )
      *piOutputCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   return s_iTxRawPowerLevelMeasurementsValues;
}

const int* tx_powers_get_mw_measured_values_for_card(int iDriverType, int iCardModel, int* piOutputCount)
{
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(iDriverType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   if ( NULL != piOutputCount )
      *piOutputCount = iCount;
   return piMwPowers;
}

int tx_powers_get_max_usable_power_mw_for_card(int iDriverType, int iCardModel)
{
   if ( _tx_powers_is_card_serial_radio(iDriverType, iCardModel) )
      return 100;
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(iDriverType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   for( int i=0; i<iCount; i++ )
   {
      if ( piMwPowers[i] <= 0 )
        return piMwPowers[i-1];
   }
   return piMwPowers[iCount-1];
}

int tx_powers_get_max_usable_power_raw_for_card(int iDriverType, int iCardModel)
{
   if ( _tx_powers_is_card_serial_radio(iDriverType, iCardModel) )
      return 20;
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(iDriverType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   for( int i=0; i<iCount; i++ )
   {
      if ( piMwPowers[i] <= 0 )
        return s_iTxRawPowerLevelMeasurementsValues[i-1];
   }
   return s_iTxRawPowerLevelMeasurementsValues[iCount-1];
}

int tx_powers_convert_raw_to_mw(int iDriverType, int iCardModel, int iRawPower)
{
   if ( _tx_powers_is_card_serial_radio(iDriverType, iCardModel) )
      return pow(10.0, ((float)iRawPower)/10.0);
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(iDriverType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   
   if ( iRawPower <= s_iTxRawPowerLevelMeasurementsValues[0] )
      return (piMwPowers[0]*iRawPower)/s_iTxRawPowerLevelMeasurementsValues[0];

   for( int i=0; i<iCount-1; i++ )
   {
      if ( piMwPowers[i+1] <= 0 )
         return piMwPowers[i];
      if ( iRawPower >= s_iTxRawPowerLevelMeasurementsValues[i] )
      if ( iRawPower <= s_iTxRawPowerLevelMeasurementsValues[i+1] )
      {
         return piMwPowers[i] + ((piMwPowers[i+1] - piMwPowers[i]) * (iRawPower - s_iTxRawPowerLevelMeasurementsValues[i])) / (s_iTxRawPowerLevelMeasurementsValues[i+1] - s_iTxRawPowerLevelMeasurementsValues[i]);
      }
   }

   return piMwPowers[0];
}

int tx_powers_convert_mw_to_raw(int iDriverType, int iCardModel, int imWPower)
{
   if ( _tx_powers_is_card_serial_radio(iDriverType, iCardModel) )
      return 10*log10((float)imWPower);
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(iDriverType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   
   if ( imWPower <= piMwPowers[0] )
   {
      int iRaw = (s_iTxRawPowerLevelMeasurementsValues[0]*imWPower)/piMwPowers[0];
      if ( iRaw < 1 )
         iRaw = 1;
      return iRaw;
   }
   for( int i=0; i<iCount-1; i++ )
   {
      if ( piMwPowers[i+1] <= 0 )
         return s_iTxRawPowerLevelMeasurementsValues[i];
      if ( imWPower >= piMwPowers[i] )
      if ( imWPower <= piMwPowers[i+1] )
      {
         return s_iTxRawPowerLevelMeasurementsValues[i] + ((s_iTxRawPowerLevelMeasurementsValues[i+1] - s_iTxRawPowerLevelMeasurementsValues[i]) * (imWPower - piMwPowers[i])) / (piMwPowers[i+1] - piMwPowers[i]);
      }
   }

   return s_iTxRawPowerLevelMeasurementsValues[0];
}

int tx_powers_get_max_usable_power_mw_for_model(Model* pModel)
{
   if ( NULL == pModel )
     return 1;

   // Select the max mw power for any present card
   int iMaxPowerMw = 10;
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      int iDriver = (pModel->radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF;
      int iCardModel = pModel->radioInterfacesParams.interface_card_model[i];
      if ( _tx_powers_is_card_serial_radio(iDriver, iCardModel) )
         continue;
      int iPowerMaxRaw = tx_powers_get_max_usable_power_raw_for_card(iDriver, iCardModel);
      int iPowerMw = tx_powers_convert_raw_to_mw(iDriver, iCardModel, iPowerMaxRaw);
      if ( iPowerMw > iMaxPowerMw )
         iMaxPowerMw = iPowerMw;
   }
   return iMaxPowerMw;
}

void tx_power_get_current_mw_powers_for_model(Model* pModel, int* piOutputArray)
{
   if ( (NULL == pModel) || (NULL == piOutputArray) )
      return;

   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      int iDriver = (pModel->radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF;
      int iCardModel = pModel->radioInterfacesParams.interface_card_model[i];
      piOutputArray[i] = tx_powers_convert_raw_to_mw(iDriver, iCardModel, pModel->radioInterfacesParams.interface_raw_power[i]);
   }
}