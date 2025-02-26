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

#include "osd_widgets_builtin.h"
#include "osd_common.h"
#include <math.h>
#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "../shared_vars.h"
#include "../shared_vars_osd.h"
#include "../shared_vars_state.h"

void _osd_widget_altitude_reset_possize(type_osd_widget* pWidgetInfo, int iModelIndex)
{
   for( int k=0; k<MODEL_MAX_OSD_PROFILES; k++ )
   {
      pWidgetInfo->display_info[iModelIndex][k].fXPos = 0.94;
      pWidgetInfo->display_info[iModelIndex][k].fYPos = 0.3;
      pWidgetInfo->display_info[iModelIndex][k].fWidth = 0.05;
      pWidgetInfo->display_info[iModelIndex][k].fHeight = 0.4;
   }
}

void osd_widget_builtin_altitude_on_new_vehicle(type_osd_widget* pWidgetInfo, u32 uCurrentVehicleId)
{
   if ( NULL == pWidgetInfo )
      return;

   int iModelIndex = osd_widget_add_to_model(pWidgetInfo, uCurrentVehicleId);
   if ( -1 == iModelIndex )
      return;

   pWidgetInfo->display_info[iModelIndex][0].bShow = true;
   if ( fabs(pWidgetInfo->display_info[iModelIndex][0].fWidth) < 0.001 )
      _osd_widget_altitude_reset_possize(pWidgetInfo, iModelIndex);
}

void osd_widget_builtin_altitude_on_main_vehicle_changed(type_osd_widget* pWidgetInfo, u32 uCurrentVehicleId)
{
   if ( NULL == pWidgetInfo )
      return;
}

void osd_widget_builtin_altitude_render(type_osd_widget* pWidgetInfo, type_osd_widget_display_info* pDisplayInfo, u32 uCurrentVehicleId, int iOSDScreen)
{
   if ( (NULL == pWidgetInfo) || (NULL == pDisplayInfo) || (iOSDScreen < 0) )
      return;
   if ( (0 == uCurrentVehicleId) || (MAX_U32 == uCurrentVehicleId) )
      return;
   if ( (NULL == g_pCurrentModel) || (iOSDScreen < 0) || (iOSDScreen >= MODEL_MAX_OSD_PROFILES) )
      return;

   int iModelIndex = osd_widget_get_model_index(pWidgetInfo, uCurrentVehicleId);
   if ( iModelIndex < 0 )
      return;

   t_structure_vehicle_info* pVInfo = get_vehicle_runtime_info_for_vehicle_id(uCurrentVehicleId);
   if ( (NULL == pVInfo) || (NULL == pVInfo->pModel) )
      return;

   if ( fabs(pWidgetInfo->display_info[iModelIndex][iOSDScreen].fWidth) < 0.001 )
      _osd_widget_altitude_reset_possize(pWidgetInfo, iModelIndex);

   osd_set_colors_background_fill();
   g_pRenderEngine->drawRect(pDisplayInfo->fXPos, pDisplayInfo->fYPos, pDisplayInfo->fWidth, pDisplayInfo->fHeight);

   osd_set_colors();
   g_pRenderEngine->setFill(0,0,0,0.2);
   g_pRenderEngine->setStrokeSize(2.0);
   g_pRenderEngine->drawRect(pDisplayInfo->fXPos, pDisplayInfo->fYPos, pDisplayInfo->fWidth, pDisplayInfo->fHeight);

   g_pRenderEngine->setColors(get_Color_OSDText());

   g_pRenderEngine->drawLine(pDisplayInfo->fXPos + 0.5*pDisplayInfo->fWidth, pDisplayInfo->fYPos, pDisplayInfo->fXPos + 0.5*pDisplayInfo->fWidth, pDisplayInfo->fYPos + pDisplayInfo->fHeight);
   g_pRenderEngine->drawLine(pDisplayInfo->fXPos + 0.5*pDisplayInfo->fWidth, pDisplayInfo->fYPos, pDisplayInfo->fXPos , pDisplayInfo->fYPos + pDisplayInfo->fHeight);
   //double c[4] = {255,0,0,0.9};
   //g_pRenderEngine->setColors(c);
}
