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
