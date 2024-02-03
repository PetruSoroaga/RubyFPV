/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "osd_widgets.h"
#include "osd_common.h"

#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "../shared_vars.h"
#include "../shared_vars_osd.h"
#include "../shared_vars_state.h"

osd_widget_t s_ListOSDWidgets[MAX_OSD_WIDGETS];
int s_ListOSDWidgetsCount = 0;

int osd_widgets_get_count()
{
   return s_ListOSDWidgetsCount;
}

osd_widget_t* osd_widgets_get(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= s_ListOSDWidgetsCount) )
      return NULL;
   return &(s_ListOSDWidgets[iIndex]);
}

bool osd_widgets_load()
{
   return true;
}

bool osd_widgets_save()
{
   return true;
}

void _osd_render_widget(int iIndex, int iModelIndex)
{
   if ( (iIndex < 0) || (iIndex >= s_ListOSDWidgetsCount) )
      return;
}

void osd_widgets_render(u32 uCurrentVehicleId)
{
   if ( (0 == uCurrentVehicleId) || (MAX_U32 == uCurrentVehicleId) )
      return;
   if ( NULL == g_pCurrentModel )
      return;

   for( int i=0; i<s_ListOSDWidgetsCount; i++ )
   {
      for( int k=0; k<MAX_MODELS; k++ )
      {
         if ( s_ListOSDWidgets[i].uShowInModels[k] == uCurrentVehicleId )
         {
            if ( s_ListOSDWidgets[i].bShowInOSDScreen[k][osd_get_current_layout_index()] )
               _osd_render_widget(i,k);
         }
      }
   }
}
