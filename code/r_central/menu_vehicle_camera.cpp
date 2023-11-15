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
#include "../base/models.h"
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "../common/string_utils.h"
#include "menu.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_camera_gains.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_calibrate_hdmi.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "timers.h"


MenuVehicleCamera::MenuVehicleCamera(void)
:Menu(MENU_ID_VEHICLE_CAMERA, "Camera Settings", NULL)
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;

   m_bDidAnyLiveUpdates = false;
   m_iLastCameraType = -1;

   char szCam[256];
   char szCam2[256];
   str_get_hardware_camera_type_string( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   snprintf(szCam, sizeof(szCam), "Camera: %s", szCam2);

   char* szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);
   if ( 0 != szCamName[0] )
   {
      strlcat(szCam, " (", sizeof(szCam));
      strlcat(szCam, szCamName, sizeof(szCam));
      strlcat(szCam, ")", sizeof(szCam));
   }
   addTopLine(szCam);

   if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType == CAMERA_TYPE_HDMI )
      addTopLine("Note: Some HDMI-CSI adapters do not support changing camera/video params on the fly!");

   addItems();

   m_ExtraHeight = 2*menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
}

MenuVehicleCamera::~MenuVehicleCamera()
{
   if ( m_bDidAnyLiveUpdates )
      saveAsCurrentModel(g_pCurrentModel);
}

void MenuVehicleCamera::addItems()
{
   float fSliderWidth = 0.12;

   removeAllItems();

   m_iLastCameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType;

   m_IndexCamera = m_IndexForceMode = -1;
   m_IndexProfile = -1;
   m_IndexBrightness = m_IndexContrast = m_IndexSaturation = m_IndexSharpness = -1;
   m_IndexAWBMode = -1;
   m_IndexEV = m_IndexEVValue = -1;
   m_IndexAGC = -1;
   m_IndexExposure = m_IndexWhiteBalance = -1;
   m_IndexAnalogGains = -1;
   m_IndexMetering = m_IndexDRC = -1;
   m_IndexISO = m_IndexISOValue = -1;
   m_IndexShutter = m_IndexShutterValue = -1;
   m_IndexWDR = -1;
   m_IndexDayNight = -1;
   m_IndexVideoStab = m_IndexFlip = m_IndexReset = -1;

   for( int i=0; i<25; i++ )
   {
      m_pItemsSlider[i] = NULL;
      m_pItemsSelect[i] = NULL;
      m_pItemsRange[i] = NULL;
   }

   char szCam[256];
   char szCam2[256];
   char* szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);

   m_pItemsSelect[11] = new MenuItemSelect("Active Camera");
   for( int i=0; i<g_pCurrentModel->iCameraCount; i++ )
   {
      szCam[0] = 0;
      szCam2[0] = 0;
      str_get_hardware_camera_type_string( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
      snprintf(szCam, sizeof(szCam), "%s", szCam2);

      szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);
      if ( 0 != szCamName[0] )
      {
         strlcat(szCam, " (", sizeof(szCam));
         strlcat(szCam, szCamName, sizeof(szCam));
         strlcat(szCam, ")", sizeof(szCam));
      }
      m_pItemsSelect[11]->addSelection(szCam);
   }
   m_pItemsSelect[11]->setIsEditable();
   m_IndexCamera = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect("Camera Type");
   m_pItemsSelect[12]->addSelection("Autodetect");
   m_pItemsSelect[12]->addSelection("CSI Camera");
   m_pItemsSelect[12]->addSelection("HDMI Camera");
   m_pItemsSelect[12]->addSelection("Veye 290");
   m_pItemsSelect[12]->addSelection("Veye 307");
   m_pItemsSelect[12]->addSelection("Veye 327");
   m_pItemsSelect[12]->addSelection("USB Camera");
   m_pItemsSelect[12]->addSelection("IP Camera");
   m_pItemsSelect[12]->setSelectionIndexDisabled(6);
   m_pItemsSelect[12]->setSelectionIndexDisabled(7);
   m_pItemsSelect[12]->setIsEditable();
   m_IndexForceMode = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[0] = new MenuItemSelect("Profile"); 
   for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
   {
      char szBuff[32];
      snprintf(szBuff, sizeof(szBuff), "Profile %s", model_getCameraProfileName(i));
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   m_pItemsSelect[0]->setIsEditable();
   m_IndexProfile = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[0] = new MenuItemSlider("Brightness", 0,100,50, fSliderWidth);
   m_IndexBrightness = addMenuItem(m_pItemsSlider[0]);
 
   m_pItemsSlider[1] = new MenuItemSlider("Contrast", 0,100,0, fSliderWidth);
   m_IndexContrast = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Saturation", -100,100,0, fSliderWidth);
   m_IndexSaturation = addMenuItem(m_pItemsSlider[2]);


   if ( g_pCurrentModel->isCameraVeye327290() || g_pCurrentModel->isCameraCSICompatible() )
   {
      if ( g_pCurrentModel->isCameraVeye327290() )
         m_pItemsSlider[3] = new MenuItemSlider("Sharpness", 0,10,0, fSliderWidth);
      else
         m_pItemsSlider[3] = new MenuItemSlider("Sharpness", -100,100,0, fSliderWidth);
      m_IndexSharpness = addMenuItem(m_pItemsSlider[3]);
   }
   else
      m_IndexSharpness = -1;

   if ( g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[15] = new MenuItemSelect("Day/Night Mode", "Sets the mode to daylight (color and IR cut) or night (black and white, no IR cut).");
      m_pItemsSelect[15]->addSelection("Daylight");
      m_pItemsSelect[15]->addSelection("Night B/W");
      m_IndexDayNight = addMenuItem(m_pItemsSelect[15]);
   }
   else
      m_IndexDayNight = -1;

   m_pItemsSelect[3] = new MenuItemSelect("White Balance");
   m_pItemsSelect[3]->addSelection("Off");
   m_pItemsSelect[3]->addSelection("Auto");
   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[3]->addSelection("Sun");
      m_pItemsSelect[3]->addSelection("Cloud");
      m_pItemsSelect[3]->addSelection("Shade");
      m_pItemsSelect[3]->addSelection("Horizont");
      m_pItemsSelect[3]->addSelection("Grey World");
   }
   m_pItemsSelect[3]->setIsEditable();
   m_IndexWhiteBalance = addMenuItem(m_pItemsSelect[3]);

   if ( g_pCurrentModel->isCameraVeye327290() )
   {
      m_pItemsSelect[14] = new MenuItemSelect("WDR mode");
      m_pItemsSelect[14]->addSelection("Off");
      m_pItemsSelect[14]->addSelection("On, Low");
      m_pItemsSelect[14]->addSelection("On, High");
      m_pItemsSelect[14]->addSelection("On, DOL");
      m_pItemsSelect[14]->setIsEditable();
      m_IndexWDR = addMenuItem(m_pItemsSelect[14]);
   }
   else
      m_IndexWDR = -1;

   //m_pItemsSelect[10] = new MenuItemSelect("AWB Mode");
   //m_pItemsSelect[10]->addSelection("Mode 1");
   //m_pItemsSelect[10]->addSelection("Mode 2");
   //m_pItemsSelect[10]->setIsEditable();
   //m_IndexAWBMode = addMenuItem(m_pItemsSelect[10]);


   if ( g_pCurrentModel->isCameraVeye327290() )
   {
      m_pItemsSlider[7] = new MenuItemSlider("AGC", "Agc stands for auto gain control. It is a part of auto exposure, in different light intensity, will try to adjust the gain of sensor to achieve the same image brightness.", 0,15,5, fSliderWidth);
      m_IndexAGC = addMenuItem(m_pItemsSlider[7]);
   }
   else
      m_IndexAGC = -1;

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_IndexAnalogGains = addMenuItem( new MenuItem("Analog Gains", "Sets the analog gains when the AWB is turned off."));
      m_pMenuItems[m_IndexAnalogGains]->showArrow();
   }
   else
      m_IndexAnalogGains = -1;

   m_pItemsSelect[7] = new MenuItemSelect("Shutter Speed", "Sets shutter speed to auto or manual.");
   m_pItemsSelect[7]->addSelection("Auto");
   m_pItemsSelect[7]->addSelection("Manual");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexShutter = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSlider[6] = new MenuItemSlider("", "Sets the shutter speed to 1/x of a second", 30,5000,1000, fSliderWidth);
   m_pItemsSlider[6]->setStep(30);
   m_IndexShutterValue = addMenuItem(m_pItemsSlider[6]);

   if ( ! g_pCurrentModel->isCameraVeye() )
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
      m_IndexExposure = addMenuItem(m_pItemsSelect[2]);
   }
   else
      m_IndexExposure = -1;

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[4] = new MenuItemSelect("Metering");
      m_pItemsSelect[4]->addSelection("Average");
      m_pItemsSelect[4]->addSelection("Spot");
      m_pItemsSelect[4]->addSelection("Backlit");
      m_pItemsSelect[4]->addSelection("Matrix");
      m_pItemsSelect[4]->setIsEditable();
      m_IndexMetering = addMenuItem(m_pItemsSelect[4]);
   }
   else
      m_IndexMetering = -1;

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[5] = new MenuItemSelect("DRC", "Dynamic Range Compensation");
      m_pItemsSelect[5]->addSelection("Off");
      m_pItemsSelect[5]->addSelection("Low");
      m_pItemsSelect[5]->addSelection("Medium");
      m_pItemsSelect[5]->addSelection("High");
      m_pItemsSelect[5]->setIsEditable();
      m_IndexDRC = addMenuItem(m_pItemsSelect[5]);
   }
   else
     m_IndexDRC = -1;

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[6] = new MenuItemSelect("ISO", "Sets manual/auto ISO.");
      m_pItemsSelect[6]->addSelection("Auto");
      m_pItemsSelect[6]->addSelection("Manual");
      m_pItemsSelect[6]->setIsEditable();
      m_IndexISO = addMenuItem(m_pItemsSelect[6]);

      m_pItemsSlider[5] = new MenuItemSlider("", "ISO Value", 100,800,400, fSliderWidth);
      m_pItemsSlider[5]->setStep(20);
      m_IndexISOValue = addMenuItem(m_pItemsSlider[5]);
   }
   else
   {
      m_IndexISO = -1;
      m_IndexISOValue = -1;
   }

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[1] = new MenuItemSelect("EV Compensation", "Sets exposure compensation to manual or auto. Manual values work only when AWB is enabled.");
      m_pItemsSelect[1]->addSelection("Auto");
      m_pItemsSelect[1]->addSelection("Manual");
      m_pItemsSelect[1]->setIsEditable();
      m_IndexEV = addMenuItem(m_pItemsSelect[1]);

      m_pItemsSlider[4] = new MenuItemSlider("", "EV Compensation value", -10,10,0, fSliderWidth);
      m_IndexEVValue = addMenuItem(m_pItemsSlider[4]);
   }
   else
   {
      m_IndexEV = -1;
      m_IndexEVValue = -1;
   }

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      m_pItemsSelect[8] = new MenuItemSelect("Video Stabilisation", "Enables video stabilisation, if supported by the camera.");
      m_pItemsSelect[8]->addSelection("Off");
      m_pItemsSelect[8]->addSelection("On");
      m_pItemsSelect[8]->setIsEditable();
      m_IndexVideoStab = addMenuItem(m_pItemsSelect[8]);
   }
   else
      m_IndexVideoStab = -1;

   m_pItemsSelect[9] = new MenuItemSelect("Flip camera", "Flips the camera video output upside down.");  
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setIsEditable();
   m_IndexFlip = addMenuItem(m_pItemsSelect[9]);

   m_IndexCalibrateHDMI = addMenuItem(new MenuItem("Calibrate HDMI output", "Calibrate the colors, brightness and contrast on the controller display."));
   m_pMenuItems[m_IndexCalibrateHDMI]->showArrow();

   m_IndexReset = addMenuItem(new MenuItem("Reset Profile", "Resets the current vehicle's camera paramters (brightness, contrast, etc) to the default values for the current profile."));
   valuesToUI();
}

void MenuVehicleCamera::updateUIValues(int iCameraProfileIndex)
{   
   bool enableGains = false;
   if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].whitebalance == 0 )
      enableGains = true;

   if ( -1 != m_IndexAnalogGains )
      m_pMenuItems[m_IndexAnalogGains]->setEnabled(enableGains);

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
   else if ( CAMERA_TYPE_USB == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(6);
   else if ( CAMERA_TYPE_IP == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      m_pItemsSelect[12]->setSelection(7);
   else
      m_pItemsSelect[12]->setSelection(0);

   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].brightness);
   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].contrast);
   m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].saturation-100);

   if ( g_pCurrentModel->isCameraVeye327290() || ( ! g_pCurrentModel->isCameraVeye()) )
   if ( NULL != m_pItemsSlider[3] )
   {
      if ( g_pCurrentModel->isCameraVeye327290() )
         m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].sharpness-100);
      else
         m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].sharpness-100);
   }

   if ( g_pCurrentModel->isCameraHDMI() )
   {
      m_pItemsSlider[0]->setEnabled(false);
      m_pItemsSlider[1]->setEnabled(false);
      m_pItemsSlider[2]->setEnabled(false);
      m_pItemsSlider[3]->setEnabled(false);
   }
   else
   {
      m_pItemsSlider[0]->setEnabled(true);
      m_pItemsSlider[1]->setEnabled(true);
      m_pItemsSlider[2]->setEnabled(true);
      m_pItemsSlider[3]->setEnabled(true);
   }

   if ( -1 != m_IndexDayNight )
   {
      if ( g_pCurrentModel->isCameraVeye() )
      {
         m_pItemsSelect[15]->setEnabled(true);
         m_pItemsSelect[15]->setSelectedIndex(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].dayNightMode );
      }
      else
         m_pItemsSelect[15]->setEnabled(false);
   }

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      if ( NULL != m_pItemsSelect[1] )
         m_pItemsSelect[1]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev != 0);
      if ( NULL != m_pItemsSlider[4] )
      {
         m_pItemsSlider[4]->setCurrentValue(((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev)-11);
         m_pItemsSlider[4]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev != 0);
      }
   }
   if ( ! g_pCurrentModel->isCameraVeye() )
   if ( (m_IndexExposure != -1) && (m_pItemsSelect[2] != NULL) )
      m_pItemsSelect[2]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].exposure);
   
   m_pItemsSelect[3]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].whitebalance);

   if ( m_IndexAWBMode != -1 )
      m_pItemsSelect[10]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].flags & CAMERA_FLAG_AWB_MODE_OLD)?0:1);

   if ( ! g_pCurrentModel->isCameraVeye() )
   if ( (m_IndexMetering != -1) && (m_pItemsSelect[4] != NULL) )
      m_pItemsSelect[4]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].metering);
   
   if ( (m_IndexDRC != -1) && (m_pItemsSelect[5] != NULL) )
      m_pItemsSelect[5]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].drc);

   if ( ! g_pCurrentModel->isCameraVeye() )
   {
      if ( NULL != m_pItemsSelect[6] )
         m_pItemsSelect[6]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso != 0);
      if ( NULL != m_pItemsSlider[5] )
      {
         m_pItemsSlider[5]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso);
         m_pItemsSlider[5]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso != 0);
      }
   }

   if ( NULL != m_pItemsSelect[7] )
      m_pItemsSelect[7]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed != 0);

   if ( NULL != m_pItemsSlider[6] )
   {
      m_pItemsSlider[6]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed);
      m_pItemsSlider[6]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].shutterspeed != 0);
   }

   if ( m_IndexAGC != -1 )
   if ( NULL != m_pItemsSlider[7] )
      m_pItemsSlider[7]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].drc);

   if ( ! g_pCurrentModel->isCameraVeye() )
   if ( -1 != m_IndexVideoStab )
   if ( NULL != m_pItemsSelect[8] )
      m_pItemsSelect[8]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].vstab!=0)?1:0);

   if ( NULL != m_pItemsSelect[9] )
      m_pItemsSelect[9]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].flip_image!=0)?1:0);

   if ( -1 != m_IndexWDR )
   if ( NULL != m_pItemsSelect[14] )
      m_pItemsSelect[14]->setSelection((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].wdr);
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
      if ( i == 0 )
         y += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
   }
   RenderEnd(yTop);
}

bool MenuVehicleCamera::canSendLiveUpdates()
{
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->hasCamera()) )
      return false;

   if ( g_pCurrentModel->isCameraCSICompatible() )
      return true;
   if ( g_pCurrentModel->isCameraVeye327290() )
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

   if ( m_IndexSaturation == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].saturation != (m_pItemsSlider[2]->getCurrentValue()+100) )
   {
      cparams.profiles[iProfile].saturation = m_pItemsSlider[2]->getCurrentValue()+100;
      bSendToVehicle = true;
   }

   if ( (m_IndexSharpness != -1 && m_IndexSharpness == itemIndex ) || itemIndex == -1 )
   if ( m_pItemsSlider[3] != NULL )
   {
      if ( g_pCurrentModel->isCameraVeye327290() )
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

   if ( m_IndexEV != -1 && m_IndexEVValue != -1 && (m_pItemsSelect[1] != NULL) && (m_pItemsSlider[4] != NULL) )
   {
      cparams.profiles[iProfile].ev = (m_pItemsSelect[1]->getSelectedIndex() == 0)?0:11;
      if ( m_pItemsSelect[1]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].ev = m_pItemsSlider[4]->getCurrentValue()+11;    
   }

   if ( (m_IndexExposure != -1) && (m_pItemsSelect[2] != NULL) )
      cparams.profiles[iProfile].exposure = m_pItemsSelect[2]->getSelectedIndex();

   if ( m_IndexWhiteBalance != -1 )
      cparams.profiles[iProfile].whitebalance = m_pItemsSelect[3]->getSelectedIndex();

   if ( m_IndexAWBMode != -1 )
   {
      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         cparams.profiles[iProfile].flags |= CAMERA_FLAG_AWB_MODE_OLD;
      else
         cparams.profiles[iProfile].flags &= ~CAMERA_FLAG_AWB_MODE_OLD;
   }

   if ( (m_IndexAGC != -1) && (m_pItemsSlider[7] != NULL) )
   {
      cparams.profiles[iProfile].drc = m_pItemsSlider[7]->getCurrentValue();
      bSendToVehicle = true;
   }
   else if ( (m_IndexDRC != -1) && (m_pItemsSelect[5] != NULL) )
      cparams.profiles[iProfile].drc = m_pItemsSelect[5]->getSelectedIndex();

   if ( (m_IndexMetering != -1) && (m_pItemsSelect[4] != NULL) )
      cparams.profiles[iProfile].metering = m_pItemsSelect[4]->getSelectedIndex();

   if ( m_IndexISO != -1 && m_IndexISOValue != -1 && (m_pItemsSelect[6] != NULL) && (m_pItemsSlider[5] != NULL) )
   {
      cparams.profiles[iProfile].iso = (m_pItemsSelect[6]->getSelectedIndex() == 0)?0:400;
      if ( m_pItemsSelect[6]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].iso = m_pItemsSlider[5]->getCurrentValue();
   }

   if ( m_IndexShutter != -1 && m_IndexShutterValue != -1 )
   {
      cparams.profiles[iProfile].shutterspeed = (m_pItemsSelect[7]->getSelectedIndex() == 0)?0:1000;
      if ( m_pItemsSelect[7]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].shutterspeed = m_pItemsSlider[6]->getCurrentValue();
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
         //saveAsCurrentModel(g_pCurrentModel);  
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
   {
      updateUIValues(m_pItemsSelect[0]->getSelectedIndex());
   }

   if ( ! m_pMenuItems[m_SelectedIndex]->isEditing() )
   {
      return;
   }

   if ( itemIndex == m_IndexCamera || itemIndex == m_IndexForceMode || itemIndex == m_IndexFlip )
      return;

   if ( ! canSendLiveUpdates() )
   {
      return;
   }

   sendCameraParams(itemIndex, true);
}

void MenuVehicleCamera::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
   if ( ! canSendLiveUpdates() )
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
      if ( tmp != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_FORCE_CAMERA_TYPE, tmp, NULL, 0) )
         valuesToUI();
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
      if ( canSendLiveUpdates() )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexContrast == m_SelectedIndex )
   {
      if ( canSendLiveUpdates() )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexSaturation == m_SelectedIndex )
   {
      if ( canSendLiveUpdates() )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexSharpness == m_SelectedIndex )
   {
      if ( canSendLiveUpdates() )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexDayNight == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexEV == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexEVValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexExposure == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexWhiteBalance == m_SelectedIndex )
      sendCameraParams(-1, false);


   if ( m_IndexAWBMode == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( g_pCurrentModel->isCameraVeye327290() )
   if ( m_IndexAGC == m_SelectedIndex )
   {
      if ( canSendLiveUpdates() )
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

   if ( m_IndexShutter == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexShutterValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexVideoStab == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexFlip == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexWDR != -1 && m_IndexWDR == m_SelectedIndex )
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