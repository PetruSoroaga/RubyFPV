/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

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
#include "menu.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_camera_gains.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_calibrate_hdmi.h"

MenuVehicleCamera::MenuVehicleCamera(void)
:Menu(MENU_ID_VEHICLE_CAMERA, "Camera Settings", NULL)
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;

   m_bDidAnyLiveUpdates = false;
   m_iLastCameraType = -1;

   resetIndexes();

   char szCam[256];
   char szCam2[256];
   str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   snprintf(szCam, 255, "Camera: %s", szCam2);

   char* szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);
   log_line("Current camera (%d) name: [%s]", g_pCurrentModel->iCurrentCamera, szCamName);
   
   if ( (NULL != szCamName) && (0 != szCamName[0]) )
   {
      strcat(szCam, " (");
      strcat(szCam, szCamName);
      strcat(szCam, ")");
   }
   addTopLine(szCam);

   if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType == CAMERA_TYPE_HDMI )
      addTopLine("Note: Some HDMI-CSI adapters do not support changing camera/video params on the fly!");

   addItems();
}

MenuVehicleCamera::~MenuVehicleCamera()
{
   if ( m_bDidAnyLiveUpdates )
      saveControllerModel(g_pCurrentModel);
}

void MenuVehicleCamera::resetIndexes()
{
   m_IndexCamera = m_IndexForceMode = -1;
   m_IndexProfile = -1;
   m_IndexBrightness = m_IndexContrast = m_IndexSaturation = m_IndexSharpness = -1;
   m_IndexHue = -1;
   m_IndexEV = m_IndexEVValue = -1;
   m_IndexAGC = -1;
   m_IndexExposureMode = m_IndexExposureValue = m_IndexWhiteBalance = -1;
   m_IndexAnalogGains = -1;
   m_IndexMetering = m_IndexDRC = -1;
   m_IndexISO = m_IndexISOValue = -1;
   m_IndexShutterMode = m_IndexShutterValue = -1;
   m_IndexWDR = -1;
   m_IndexDayNight = -1;
   m_IndexVideoStab = m_IndexFlip = m_IndexReset = -1;
   m_IndexIRCut = -1;
   m_IndexOpenIPCDayNight = -1;
   m_IndexOpenIPC3A = -1;
}

void MenuVehicleCamera::addItems()
{
   float fSliderWidth = 0.12 * Menu::getScaleFactor();
   float fMargin = 0.01 * Menu::getScaleFactor();

   log_line("MenuCamera: Total cameras detected: %d", g_pCurrentModel->iCameraCount);
   log_line("MenuCamera: Active camera: %d", g_pCurrentModel->iCurrentCamera+1);

   for( int i=0; i<g_pCurrentModel->iCameraCount; i++ )
   {
      char szBuff1[128];
      char szBuff2[128];
      str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[i].iCameraType, szBuff1);
      str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[i].iForcedCameraType, szBuff2);
      log_line("MenuCamera: Camera %d hardware type: %s, overwrite type: %s, current profile: %d", i+1, szBuff1, szBuff2, g_pCurrentModel->camera_params[i].iCurrentProfile);
   }

   removeAllItems();

   for( int i=0; i<25; i++ )
   {
      m_pItemsSlider[i] = NULL;
      m_pItemsSelect[i] = NULL;
      m_pItemsRange[i] = NULL;
   }

   m_iLastCameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType;

   resetIndexes();

   for( int i=0; i<25; i++ )
   {
      m_pItemsSlider[i] = NULL;
      m_pItemsSelect[i] = NULL;
      m_pItemsRange[i] = NULL;
   }

   char szCam[256];
   char szCam2[256];
   char* szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);

   m_pItemsSelect[11] = new MenuItemSelect("Active Camera", "Selects which camera should be active, if multiple cameras are present on this vehicle.");
   for( int i=0; i<g_pCurrentModel->iCameraCount; i++ )
   {
      szCam[0] = 0;
      szCam2[0] = 0;
      str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
      sprintf(szCam, "%s", szCam2);

      szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);
      if ( (NULL != szCamName) && (0 != szCamName[0]) )
      if ( NULL == strstr(szCam, szCamName) )
      {
         strcat(szCam, " (");
         strcat(szCam, szCamName);
         strcat(szCam, ")");
      }
      m_pItemsSelect[11]->addSelection(szCam);
   }
   m_pItemsSelect[11]->setIsEditable();
   m_pItemsSelect[11]->setExtraHeight(0.8*m_sfMenuPaddingY);
   m_IndexCamera = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect("Camera Type", "Autodetect the active camera type or force a particular camera type for the active camera.");
   m_pItemsSelect[12]->addSelection("Autodetect");
   m_pItemsSelect[12]->addSelection("CSI Camera", !g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("HDMI Camera", !g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("Veye 290", !g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("Veye 307", !g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("Veye 327", !g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("OpenIPC IMX307", g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("OpenIPC IMX335", g_pCurrentModel->isRunningOnOpenIPCHardware());
   m_pItemsSelect[12]->addSelection("OpenIPC IMX415", g_pCurrentModel->isRunningOnOpenIPCHardware());
   //m_pItemsSelect[12]->addSelection("USB Camera", false);
   m_pItemsSelect[12]->addSelection("IP Camera", false);
   m_pItemsSelect[12]->setIsEditable();
   m_IndexForceMode = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[0] = new MenuItemSelect("Profile"); 
   for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
   {
      char szBuff[32];
      sprintf(szBuff, "Profile %s", model_getCameraProfileName(i));
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   m_pItemsSelect[0]->setIsEditable();
   m_IndexProfile = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[0] = new MenuItemSlider("Brightness", 0,100,50, fSliderWidth);
   m_pItemsSlider[0]->setMargin(fMargin);
   m_IndexBrightness = addMenuItem(m_pItemsSlider[0]);
 
   m_pItemsSlider[1] = new MenuItemSlider("Contrast", 0,100,0, fSliderWidth);
   m_pItemsSlider[1]->setMargin(fMargin);
   m_IndexContrast = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Saturation", -100,100,0, fSliderWidth);
   m_pItemsSlider[2]->setMargin(fMargin);
   m_IndexSaturation = addMenuItem(m_pItemsSlider[2]);

   if ( g_pCurrentModel->isActiveCameraVeye307() ||
        g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      m_pItemsSlider[8] = new MenuItemSlider("Hue", 0,100,0, fSliderWidth);
      m_pItemsSlider[8]->setMargin(fMargin);
      m_IndexHue = addMenuItem(m_pItemsSlider[8]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() || g_pCurrentModel->isActiveCameraCSICompatible() )
   {
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
         m_pItemsSlider[3] = new MenuItemSlider("Sharpness", 0,10,0, fSliderWidth);
      else
         m_pItemsSlider[3] = new MenuItemSlider("Sharpness", -100,100,0, fSliderWidth);
      m_pItemsSlider[3]->setMargin(fMargin);
      m_IndexSharpness = addMenuItem(m_pItemsSlider[3]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye() )
   {
      m_pItemsSelect[15] = new MenuItemSelect("Day/Night Mode", "Sets the mode to daylight (color and IR cut) or night (black and white, no IR cut).");
      m_pItemsSelect[15]->addSelection("Daylight");
      m_pItemsSelect[15]->addSelection("Night B/W");
      m_pItemsSelect[15]->setMargin(fMargin);
      m_IndexDayNight = addMenuItem(m_pItemsSelect[15]);
   }

   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
   {
      m_pItemsSelect[3] = new MenuItemSelect("White Balance");
      m_pItemsSelect[3]->addSelection("Off");
      m_pItemsSelect[3]->addSelection("Auto");
      if ( ! g_pCurrentModel->isActiveCameraVeye() )
      {
         m_pItemsSelect[3]->addSelection("Sun");
         m_pItemsSelect[3]->addSelection("Cloud");
         m_pItemsSelect[3]->addSelection("Shade");
         m_pItemsSelect[3]->addSelection("Horizont");
         m_pItemsSelect[3]->addSelection("Grey World");
      }
      m_pItemsSelect[3]->setIsEditable();
      m_pItemsSelect[3]->setMargin(fMargin);
      m_IndexWhiteBalance = addMenuItem(m_pItemsSelect[3]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   {
      m_pItemsSelect[14] = new MenuItemSelect("WDR mode");
      m_pItemsSelect[14]->addSelection("Off");
      m_pItemsSelect[14]->addSelection("On, Low");
      m_pItemsSelect[14]->addSelection("On, High");
      m_pItemsSelect[14]->addSelection("On, DOL");
      m_pItemsSelect[14]->setIsEditable();
      m_pItemsSelect[14]->setMargin(fMargin);
      m_IndexWDR = addMenuItem(m_pItemsSelect[14]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   {
      m_pItemsSlider[7] = new MenuItemSlider("AGC", "Agc stands for auto gain control. It is a part of auto exposure, in different light intensity, will try to adjust the gain of sensor to achieve the same image brightness.", 0,15,5, fSliderWidth);
      m_pItemsSlider[7]->setMargin(fMargin);
      m_IndexAGC = addMenuItem(m_pItemsSlider[7]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_IndexAnalogGains = addMenuItem( new MenuItem("Analog Gains", "Sets the analog gains when the AWB is turned off."));
      m_pMenuItems[m_IndexAnalogGains]->showArrow();
      m_pMenuItems[m_IndexAnalogGains]->setMargin(fMargin);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() || g_pCurrentModel->isActiveCameraVeye() )
   {
      m_pItemsSelect[7] = new MenuItemSelect("Shutter Speed", "Sets shutter speed to auto or manual.");
      m_pItemsSelect[7]->addSelection("Auto");
      m_pItemsSelect[7]->addSelection("Manual");
      m_pItemsSelect[7]->setIsEditable();
      m_pItemsSelect[7]->setMargin(fMargin);
      m_IndexShutterMode = addMenuItem(m_pItemsSelect[7]);
   
      m_pItemsSlider[6] = new MenuItemSlider("", "Sets the shutter speed to 1/x of a second", 30,5000,1000, fSliderWidth);
      m_pItemsSlider[6]->setStep(30);
      m_pItemsSlider[6]->setMargin(fMargin);
      m_IndexShutterValue = addMenuItem(m_pItemsSlider[6]);
   }
   else if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      m_pItemsSelect[22] = new MenuItemSelect("3A Algorithms", "Sets 3A algorithms (autoexposure, autowhitebalance, autofocus) used by the camera ISP processor.");
      m_pItemsSelect[22]->addSelection("Default");
      m_pItemsSelect[22]->addSelection("Sigmastar");
      m_pItemsSelect[22]->setIsEditable();
      m_pItemsSelect[22]->setMargin(fMargin);
      m_IndexOpenIPC3A = addMenuItem(m_pItemsSelect[22]);

      m_pItemsSelect[7] = new MenuItemSelect("Shutter Speed", "Sets the shutter speed to be auto controllerd by camera or manula set by user.");  
      m_pItemsSelect[7]->addSelection("Auto");
      m_pItemsSelect[7]->addSelection("Manual");
      m_pItemsSelect[7]->setIsEditable();
      m_pItemsSelect[7]->setMargin(fMargin);
      m_IndexShutterMode = addMenuItem(m_pItemsSelect[7]);

      m_pItemsSlider[6] = new MenuItemSlider("", "Sets the camera shutter speed, in miliseconds.", 1,100,10, fSliderWidth);
      m_pItemsSlider[6]->setMargin(fMargin);
      m_IndexShutterValue = addMenuItem(m_pItemsSlider[6]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[2] = new MenuItemSelect("Exposure");  
      m_pItemsSelect[2]->addSelection("Auto");
      m_pItemsSelect[2]->addSelection("Night");
      m_pItemsSelect[2]->addSelection("Back Light");
      m_pItemsSelect[2]->addSelection("Sports");
      m_pItemsSelect[2]->addSelection("Very Long");
      m_pItemsSelect[2]->addSelection("Fixed FPS");
      m_pItemsSelect[2]->addSelection("Antishake");
      m_pItemsSelect[2]->addSelection("Off");
      m_pItemsSelect[2]->setIsEditable();
      m_pItemsSelect[2]->setMargin(fMargin);
      m_IndexExposureMode = addMenuItem(m_pItemsSelect[2]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[4] = new MenuItemSelect("Metering");
      m_pItemsSelect[4]->addSelection("Average");
      m_pItemsSelect[4]->addSelection("Spot");
      m_pItemsSelect[4]->addSelection("Backlit");
      m_pItemsSelect[4]->addSelection("Matrix");
      m_pItemsSelect[4]->setIsEditable();
      m_pItemsSelect[4]->setMargin(fMargin);
      m_IndexMetering = addMenuItem(m_pItemsSelect[4]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[5] = new MenuItemSelect("DRC", "Dynamic Range Compensation");
      m_pItemsSelect[5]->addSelection("Off");
      m_pItemsSelect[5]->addSelection("Low");
      m_pItemsSelect[5]->addSelection("Medium");
      m_pItemsSelect[5]->addSelection("High");
      m_pItemsSelect[5]->setIsEditable();
      m_pItemsSelect[5]->setMargin(fMargin);
      m_IndexDRC = addMenuItem(m_pItemsSelect[5]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[6] = new MenuItemSelect("ISO", "Sets manual/auto ISO.");
      m_pItemsSelect[6]->addSelection("Auto");
      m_pItemsSelect[6]->addSelection("Manual");
      m_pItemsSelect[6]->setIsEditable();
      m_pItemsSelect[6]->setMargin(fMargin);
      m_IndexISO = addMenuItem(m_pItemsSelect[6]);

      m_pItemsSlider[5] = new MenuItemSlider("", "ISO Value", 100,800,400, fSliderWidth);
      m_pItemsSlider[5]->setStep(20);
      m_pItemsSlider[5]->setMargin(fMargin);
      m_IndexISOValue = addMenuItem(m_pItemsSlider[5]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[1] = new MenuItemSelect("EV Compensation", "Sets exposure compensation to manual or auto. Manual values work only when AWB is enabled.");
      m_pItemsSelect[1]->addSelection("Auto");
      m_pItemsSelect[1]->addSelection("Manual");
      m_pItemsSelect[1]->setIsEditable();
      m_pItemsSelect[1]->setMargin(fMargin);
      m_IndexEV = addMenuItem(m_pItemsSelect[1]);

      m_pItemsSlider[4] = new MenuItemSlider("", "EV Compensation value", -10,10,0, fSliderWidth);
      m_pItemsSlider[4]->setMargin(fMargin);
      m_IndexEVValue = addMenuItem(m_pItemsSlider[4]);
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[8] = new MenuItemSelect("Video Stabilisation", "Enables video stabilisation, if supported by the camera.");
      m_pItemsSelect[8]->addSelection("Off");
      m_pItemsSelect[8]->addSelection("On");
      m_pItemsSelect[8]->setIsEditable();
      m_pItemsSelect[8]->setMargin(fMargin);
      m_IndexVideoStab = addMenuItem(m_pItemsSelect[8]);
   }

   m_pItemsSelect[9] = new MenuItemSelect("Flip camera", "Flips the camera video output upside down.");  
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setIsEditable();
   m_pItemsSelect[9]->setMargin(fMargin);
   m_IndexFlip = addMenuItem(m_pItemsSelect[9]);


   m_IndexIRCut = -1;
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      m_pItemsSelect[20] = new MenuItemSelect("IR Cut", "Turn IR cut filter on/off");  
      m_pItemsSelect[20]->addSelection("On");
      m_pItemsSelect[20]->addSelection("Off");
      m_pItemsSelect[20]->setIsEditable();
      m_pItemsSelect[20]->setMargin(fMargin);
      m_IndexIRCut = addMenuItem(m_pItemsSelect[20]);
   }

   m_IndexOpenIPCDayNight = -1;
   if (g_pCurrentModel->isRunningOnOpenIPCHardware())
   if (g_pCurrentModel->isActiveCameraOpenIPC())
   {
      m_pItemsSelect[21] = new MenuItemSelect("Day/Night Mode", "Sets the mode to daylight (color) or night (black and white).");
      m_pItemsSelect[21]->addSelection("Daylight");
      m_pItemsSelect[21]->addSelection("Night B/W");
      m_pItemsSelect[21]->setIsEditable();
      m_pItemsSelect[21]->setMargin(fMargin);
      m_IndexOpenIPCDayNight = addMenuItem(m_pItemsSelect[21]);
   }

   m_IndexReset = addMenuItem(new MenuItem("Reset Profile", "Resets the current vehicle's camera paramters (brightness, contrast, etc) to the default values for the current profile."));
   m_pMenuItems[m_IndexReset]->setMargin(fMargin);

   m_IndexCalibrateHDMI = addMenuItem(new MenuItem("Calibrate HDMI output", "Calibrate the colors, brightness and contrast on the controller display."));
   m_pMenuItems[m_IndexCalibrateHDMI]->showArrow();

   log_line("MenuCamera: Added items.");

   valuesToUI();
   log_line("MenuCamera: Updated items.");
}

void MenuVehicleCamera::updateUIValues(int iCameraProfileIndex)
{   
   if ( NULL != m_pItemsSelect[11] )
      m_pItemsSelect[11]->setSelection(g_pCurrentModel->iCurrentCamera);

   if ( 0 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(0);
   else if ( CAMERA_TYPE_CSI == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(1);
   else if ( CAMERA_TYPE_HDMI == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(2);
   else if ( CAMERA_TYPE_VEYE290 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(3);
   else if ( CAMERA_TYPE_VEYE307 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(4);
   else if ( CAMERA_TYPE_VEYE327 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(5);
   else if ( CAMERA_TYPE_OPENIPC_IMX307 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(6);
   else if ( CAMERA_TYPE_OPENIPC_IMX335 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(7);
   else if ( CAMERA_TYPE_OPENIPC_IMX415 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(8);
   else if ( CAMERA_TYPE_IP == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(10);
   else
      m_pItemsSelect[12]->setSelection(0);

   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].brightness);
   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].contrast);
   m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].saturation-100);

   if ( g_pCurrentModel->isActiveCameraVeye327290() || ( ! g_pCurrentModel->isActiveCameraVeye()) )
   if ( (NULL != m_pItemsSlider[3]) && (-1 != m_IndexSharpness) )
   {
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
         m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].sharpness-100);
      else
         m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].sharpness-100);
   }

   if ( g_pCurrentModel->isActiveCameraHDMI() )
   {
      m_pItemsSlider[0]->setEnabled(false);
      m_pItemsSlider[1]->setEnabled(false);
      m_pItemsSlider[2]->setEnabled(false);
      if ( NULL != m_pItemsSlider[3] )
         m_pItemsSlider[3]->setEnabled(false);
   }
   else
   {
      m_pItemsSlider[0]->setEnabled(true);
      m_pItemsSlider[1]->setEnabled(true);
      m_pItemsSlider[2]->setEnabled(true);
      if ( NULL != m_pItemsSlider[3] )
         m_pItemsSlider[3]->setEnabled(true);
   }

   if ( (-1 != m_IndexHue) && (NULL != m_pItemsSlider[8]) )
   {
      m_pItemsSlider[8]->setEnabled(true);
      m_pItemsSlider[8]->setCurrentValue((int)(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].hue));
   }

   if ( (-1 != m_IndexDayNight) && (NULL != m_pItemsSelect[15]) )
   {
      if ( g_pCurrentModel->isActiveCameraVeye() )
      {
         m_pItemsSelect[15]->setEnabled(true);
         m_pItemsSelect[15]->setSelectedIndex(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].dayNightMode );
      }
      else
         m_pItemsSelect[15]->setEnabled(false);
   }

   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( -1 != m_IndexOpenIPC3A )
   if ( NULL != m_pItemsSelect[22] )
   {
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].uFlags & CAMERA_FLAG_OPENIPC_3A_SIGMASTAR )
         m_pItemsSelect[22]->setSelectedIndex(1);
      else
         m_pItemsSelect[22]->setSelectedIndex(0);
   }
   if ( (-1 != m_IndexWhiteBalance) && (NULL != m_pItemsSelect[3]) )
      m_pItemsSelect[3]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].whitebalance);

   if ( (-1 != m_IndexWDR) && (NULL != m_pItemsSelect[14]) )
      m_pItemsSelect[14]->setSelection((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].wdr);

   if ( (m_IndexAGC != -1) && (NULL != m_pItemsSlider[7]) )
      m_pItemsSlider[7]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].drc);

   bool enableGains = false;
   if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].whitebalance == 0 )
      enableGains = true;

   if ( -1 != m_IndexAnalogGains )
      m_pMenuItems[m_IndexAnalogGains]->setEnabled(enableGains);

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
      if ( -1 != m_IndexShutterMode )
      {
         if ( 0 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed )
         {
            m_pItemsSelect[7]->setSelectedIndex(0);
            if ( m_pItemsSlider[6] != NULL )
            {
               m_pItemsSlider[6]->setEnabled(false);
            }
         } 
         else
         {
            m_pItemsSelect[7]->setSelectedIndex(1);
            if ( (m_IndexShutterValue != -1) && (m_pItemsSlider[6] != NULL) )
            {
               m_pItemsSlider[6]->setEnabled(true);
               m_pItemsSlider[6]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed);
            }
         }
      }
   }
   else
   {
      if ( (-1 != m_IndexShutterMode) && (NULL != m_pItemsSelect[7]) )
         m_pItemsSelect[7]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed != 0);

      if ( (-1 != m_IndexShutterValue) && (NULL != m_pItemsSlider[6]) )
      {
         m_pItemsSlider[6]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed);
         m_pItemsSlider[6]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed != 0);
      }
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   if ( (m_IndexExposureMode != -1) && (m_pItemsSelect[2] != NULL) )
      m_pItemsSelect[2]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].exposure);
   
   if ( (m_IndexMetering != -1) && (m_pItemsSelect[4] != NULL) )
      m_pItemsSelect[4]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].metering);
   
   if ( (m_IndexDRC != -1) && (m_pItemsSelect[5] != NULL) )
      m_pItemsSelect[5]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].drc);

   if ( (-1 != m_IndexISO) && (NULL != m_pItemsSelect[6]) )
      m_pItemsSelect[6]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso != 0);
   
   if ( (-1 != m_IndexISOValue) && (NULL != m_pItemsSlider[5]) )
   {
      m_pItemsSlider[5]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso);
      m_pItemsSlider[5]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso != 0);
   }

   if ( (-1 != m_IndexEV) && (NULL != m_pItemsSelect[1]) )
      m_pItemsSelect[1]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev != 0);
 
   if ( (-1 != m_IndexEVValue) && (NULL != m_pItemsSlider[4]) )
   {
      m_pItemsSlider[4]->setCurrentValue(((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev)-11);
      m_pItemsSlider[4]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev != 0);
   }

   if ( (-1 != m_IndexVideoStab) && (NULL != m_pItemsSelect[8]) )
      m_pItemsSelect[8]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].vstab!=0)?1:0);

   if ( (-1 != m_IndexFlip) && (NULL != m_pItemsSelect[9]) )
      m_pItemsSelect[9]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].flip_image!=0)?1:0);

   if ( (-1 != m_IndexIRCut) && (NULL != m_pItemsSelect[20]) )
   {
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].uFlags & CAMERA_FLAG_IR_FILTER_OFF )
         m_pItemsSelect[20]->setSelectedIndex(1);
      else
         m_pItemsSelect[20]->setSelectedIndex(0);
   }

   if ( (-1 != m_IndexOpenIPCDayNight) && (NULL != m_pItemsSelect[21]) )
   {
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].uFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF )
         m_pItemsSelect[21]->setSelectedIndex(1);
      else
         m_pItemsSelect[21]->setSelectedIndex(0);
   }
}

void MenuVehicleCamera::valuesToUI()
{
   if ( m_iLastCameraType == -1 || m_iLastCameraType != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      addItems();
   m_pItemsSelect[0]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile);
   updateUIValues(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile);
}


void MenuVehicleCamera::Render()
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

bool MenuVehicleCamera::canSendLiveUpdates(int iItemIndex)
{
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->hasCamera()) )
      return false;

   if ( iItemIndex != m_IndexBrightness )
   if ( iItemIndex != m_IndexContrast )
   if ( iItemIndex != m_IndexSaturation )
   if ( iItemIndex != m_IndexSharpness )
   if ( iItemIndex != m_IndexHue )
   if ( (iItemIndex != m_IndexAGC) || (! g_pCurrentModel->isActiveCameraVeye327290()) )
      return false;

   if ( g_pCurrentModel->isActiveCameraCSI() )
      return true;

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
      return true;

   if ( g_pCurrentModel->isActiveCameraVeye307() )
      return true;

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( ! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
      return true;
     
   return false;
}

void MenuVehicleCamera::sendCameraParams(int itemIndex, bool bQuick)
{
   type_camera_parameters cparams;
   memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   bool bSendToVehicle = false;

   if ( m_IndexBrightness == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].brightness != m_pItemsSlider[0]->getCurrentValue() )
   {
      cparams.profiles[iProfile].brightness = m_pItemsSlider[0]->getCurrentValue();
      bSendToVehicle = true;
   }

   if ( m_IndexContrast == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].contrast != m_pItemsSlider[1]->getCurrentValue() )
   {
      cparams.profiles[iProfile].contrast = m_pItemsSlider[1]->getCurrentValue();
      bSendToVehicle = true;
   }

   if ( (m_IndexHue == itemIndex) && (m_IndexHue != -1) )
   if ( NULL != m_pItemsSlider[8] )
   if ( (int)(cparams.profiles[iProfile].hue) != m_pItemsSlider[8]->getCurrentValue() )
   {
      cparams.profiles[iProfile].hue = m_pItemsSlider[8]->getCurrentValue();
      bSendToVehicle = true;
   }

   if ( m_IndexSaturation == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].saturation != (m_pItemsSlider[2]->getCurrentValue()+100) )
   {
      cparams.profiles[iProfile].saturation = m_pItemsSlider[2]->getCurrentValue()+100;
      bSendToVehicle = true;
   }

   if ( -1 != m_IndexIRCut )
   if ( (m_IndexIRCut == itemIndex) || (itemIndex == -1) )
   {    
      if ( 0 == m_pItemsSelect[20]->getSelectedIndex() )
         cparams.profiles[iProfile].uFlags &= ~CAMERA_FLAG_IR_FILTER_OFF;
      else
         cparams.profiles[iProfile].uFlags |= CAMERA_FLAG_IR_FILTER_OFF;
      bSendToVehicle = true;
   }

   if ( -1 != m_IndexOpenIPC3A )
   if ( NULL != m_pItemsSelect[22] )
   {
      if ( 0 == m_pItemsSelect[22]->getSelectedIndex() )
         cparams.profiles[iProfile].uFlags &= ~CAMERA_FLAG_OPENIPC_3A_SIGMASTAR;
      else
         cparams.profiles[iProfile].uFlags |= CAMERA_FLAG_OPENIPC_3A_SIGMASTAR; 
   }
   if (-1 != m_IndexOpenIPCDayNight)
   if ((m_IndexOpenIPCDayNight == itemIndex) || (itemIndex == -1))
   {
      if (0 == m_pItemsSelect[21]->getSelectedIndex())
         cparams.profiles[iProfile].uFlags &= ~CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF;
      else
         cparams.profiles[iProfile].uFlags |= CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF;
      bSendToVehicle = true;
   }

   if ( (m_IndexSharpness != -1 && m_IndexSharpness == itemIndex ) || itemIndex == -1 )
   if ( m_pItemsSlider[3] != NULL )
   {
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
      {
         if ( cparams.profiles[iProfile].sharpness != (m_pItemsSlider[3]->getCurrentValue()+100) )
         {
            cparams.profiles[iProfile].sharpness = m_pItemsSlider[3]->getCurrentValue()+100;
            bSendToVehicle = true;
         }
      }
      else
      {
         if ( cparams.profiles[iProfile].sharpness != (m_pItemsSlider[3]->getCurrentValue()+100) )
         {
            cparams.profiles[iProfile].sharpness = m_pItemsSlider[3]->getCurrentValue()+100;
            bSendToVehicle = true;
         }
      }
   }

   if ( m_IndexDayNight != -1 )
   if ( m_IndexDayNight == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].dayNightMode != (m_pItemsSelect[15]->getSelectedIndex()) )
   {
      cparams.profiles[iProfile].dayNightMode = m_pItemsSelect[15]->getSelectedIndex();
      bSendToVehicle = true;
   }

   if ( (m_IndexEV != -1) && (m_IndexEVValue != -1) && (m_pItemsSelect[1] != NULL) && (m_pItemsSlider[4] != NULL) )
   {
      cparams.profiles[iProfile].ev = (m_pItemsSelect[1]->getSelectedIndex() == 0)?0:11;
      if ( m_pItemsSelect[1]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].ev = m_pItemsSlider[4]->getCurrentValue()+11;    
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   if ( (m_IndexExposureMode != -1) && (m_pItemsSelect[2] != NULL) )
      cparams.profiles[iProfile].exposure = m_pItemsSelect[2]->getSelectedIndex();

   if ( m_IndexWhiteBalance != -1 )
      cparams.profiles[iProfile].whitebalance = m_pItemsSelect[3]->getSelectedIndex();

   if ( (m_IndexAGC != -1) && (m_pItemsSlider[7] != NULL) )
   {
      cparams.profiles[iProfile].drc = m_pItemsSlider[7]->getCurrentValue();
      bSendToVehicle = true;
   }
   else if ( (m_IndexDRC != -1) && (m_pItemsSelect[5] != NULL) )
      cparams.profiles[iProfile].drc = m_pItemsSelect[5]->getSelectedIndex();

   if ( (m_IndexMetering != -1) && (m_pItemsSelect[4] != NULL) )
      cparams.profiles[iProfile].metering = m_pItemsSelect[4]->getSelectedIndex();

   if ( (m_IndexISO != -1) && (m_IndexISOValue != -1) && (m_pItemsSelect[6] != NULL) && (m_pItemsSlider[5] != NULL) )
   {
      cparams.profiles[iProfile].iso = (m_pItemsSelect[6]->getSelectedIndex() == 0)?0:400;
      if ( m_pItemsSelect[6]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].iso = m_pItemsSlider[5]->getCurrentValue();
   }

   if ( (m_IndexShutterMode != -1) && (m_IndexShutterValue != -1) )
   {
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      {
         cparams.profiles[iProfile].shutterspeed = 0;
         if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
            cparams.profiles[iProfile].shutterspeed = 0;
         else if ( m_pItemsSlider[6] != NULL )
            cparams.profiles[iProfile].shutterspeed = m_pItemsSlider[6]->getCurrentValue();

         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
         if ( g_pCurrentModel->validate_fps_and_exposure_settings(&g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], &cparams.profiles[iProfile]))
            addMessage("You camera exposure time was adjusted to accomodate the currently set video FPS.");
      }
      else
      {
         cparams.profiles[iProfile].shutterspeed = (m_pItemsSelect[7]->getSelectedIndex() == 0)?0:1000;
         if ( m_pItemsSelect[7]->getSelectedIndex() != 0 )
            cparams.profiles[iProfile].shutterspeed = m_pItemsSlider[6]->getCurrentValue();
      }
   }

   if ( (m_IndexVideoStab != -1) && (m_pItemsSelect[8] != NULL) )
      cparams.profiles[iProfile].vstab = m_pItemsSelect[8]->getSelectedIndex();

   if ( (m_IndexWDR != -1) && (m_pItemsSelect[14] != NULL) )
      cparams.profiles[iProfile].wdr = m_pItemsSelect[14]->getSelectedIndex();
  
   if ( NULL != m_pItemsSelect[9] )
      cparams.profiles[iProfile].flip_image = m_pItemsSelect[9]->getSelectedIndex();

   if ( bQuick )
   {
      if ( bSendToVehicle )
      {
         m_bDidAnyLiveUpdates = true;
         memcpy(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &cparams, sizeof(type_camera_parameters));
         handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters));
         //saveControllerModel(g_pCurrentModel);  
      }
   }
   else
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }
}

void MenuVehicleCamera::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   if ( m_IndexProfile == itemIndex )
      updateUIValues(m_pItemsSelect[0]->getSelectedIndex());

   if ( ! m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( itemIndex == m_IndexCamera || itemIndex == m_IndexForceMode || itemIndex == m_IndexFlip )
      return;

   if ( ! canSendLiveUpdates(itemIndex) )
      return;

   sendCameraParams(itemIndex, true);
}

void MenuVehicleCamera::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
   if ( ! canSendLiveUpdates(itemIndex) )
      return;

   sendCameraParams(itemIndex, true);
}


void MenuVehicleCamera::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   g_TimeLastVideoCameraChangeCommand = g_TimeNow;

   if ( m_IndexCamera == m_SelectedIndex )
   {
      int tmp = m_pItemsSelect[11]->getSelectedIndex();
      if ( tmp != g_pCurrentModel->iCurrentCamera )
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CURRENT_CAMERA, tmp, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexForceMode == m_SelectedIndex )
   {
      int tmp = m_pItemsSelect[12]->getSelectedIndex();
      int iCamType = 0;
      switch(tmp)
      {
         case 0: iCamType = 0; break;
         case 1: iCamType = CAMERA_TYPE_CSI; break;
         case 2: iCamType = CAMERA_TYPE_HDMI; break;
         case 3: iCamType = CAMERA_TYPE_VEYE290; break;
         case 4: iCamType = CAMERA_TYPE_VEYE307; break;
         case 5: iCamType = CAMERA_TYPE_VEYE327; break;
         case 6: iCamType = CAMERA_TYPE_OPENIPC_IMX307; break;
         case 7: iCamType = CAMERA_TYPE_OPENIPC_IMX335; break;
         case 8: iCamType = CAMERA_TYPE_OPENIPC_IMX415; break;
      }
      if ( iCamType != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      {
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_FORCE_CAMERA_TYPE, iCamType, NULL, 0) )
            valuesToUI();
         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
            addMessage("Vehicle will reboot to update the firmware state.");
      }
      return;
   }

   if ( m_IndexProfile == m_SelectedIndex)
   {
      int tmp = m_pItemsSelect[0]->getSelectedIndex();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PROFILE, tmp, NULL, 0) )
         valuesToUI();
   }

   if ( m_IndexBrightness == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexContrast == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexSaturation == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( (-1 != m_IndexHue) && (m_IndexHue == m_SelectedIndex) )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexSharpness == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexDayNight == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexEV == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexEVValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( -1 != m_IndexExposureMode )
   if ( m_IndexExposureMode == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( -1 != m_IndexExposureValue )
   if ( m_IndexExposureValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexWhiteBalance == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   if ( m_IndexAGC == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }
   if ( m_IndexAnalogGains == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleCameraGains());
      return;
   }

   if ( m_IndexMetering == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexDRC == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexISO == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexISOValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( -1 !=  m_IndexShutterMode )
   if ( m_IndexShutterMode == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( -1 != m_IndexShutterValue )
   if ( m_IndexShutterValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexVideoStab == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexFlip == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexWDR != -1 && m_IndexWDR == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexIRCut != -1 )
   if ( m_IndexIRCut == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexOpenIPC3A != -1 )
   if ( m_IndexOpenIPC3A == m_SelectedIndex )
      sendCameraParams(-1, false);

   if (m_IndexOpenIPCDayNight != -1)
   if (m_IndexOpenIPCDayNight == m_SelectedIndex)
      sendCameraParams(-1, false);

   if ( m_IndexCalibrateHDMI == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuCalibrateHDMI());      
      return;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      g_pCurrentModel->resetCameraToDefaults(g_pCurrentModel->iCurrentCamera);
      handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera])), sizeof(type_camera_parameters));
   }
}