#include "favorites.h"
#include "../base/config_file_names.h"

#define MAX_FAVORITES 10
u32 s_uListFavorites[MAX_FAVORITES];
int s_iFavoritesCount = 0;
bool s_bFavoritesLoaded = false;

bool load_favorites()
{
   for( int i=0; i<MAX_FAVORITES; i++ )
      s_uListFavorites[i] = 0;
   s_iFavoritesCount = 0;
   s_bFavoritesLoaded = true;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_FAVORITES_VEHICLES);
   FILE* fd = fopen(szFile, "rb");
   if ( NULL == fd )
      return false;

   for( int i=0; i<MAX_FAVORITES; i++ )
   {
       if ( 1 != fscanf(fd, "%u", &s_uListFavorites[i]) )
          break;
       if ( 0 == s_uListFavorites[i] )
       {
          s_iFavoritesCount = i;
          break;
       }
   }
   fclose(fd);
   return true;
}

bool save_favorites()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_FAVORITES_VEHICLES);
   FILE* fd = fopen(szFile, "wb");
   if ( NULL == fd )
      return false;

   for( int i=0; i<MAX_FAVORITES; i++ )
      fprintf(fd, "%u ", s_uListFavorites[i]);

   fclose(fd);
   return true;
}

bool vehicle_is_favorite(u32 uVehicleId)
{
   if ( ! s_bFavoritesLoaded )
      load_favorites();

   for( int i=0; i<s_iFavoritesCount; i++ )
      if ( uVehicleId == s_uListFavorites[i] )
         return true;
   return false;
}

bool remove_favorite(u32 uVehicleId)
{
   if ( ! s_bFavoritesLoaded )
      load_favorites();

   for( int i=0; i<s_iFavoritesCount; i++ )
   {
      if ( uVehicleId != s_uListFavorites[i] )
         continue;

      for( int k=i; k<s_iFavoritesCount-1; k++ )
         s_uListFavorites[k] = s_uListFavorites[k+1];
      s_uListFavorites[s_iFavoritesCount-1] = 0;
      s_iFavoritesCount--;
      save_favorites();
      break;
   }
   return true;
}

bool add_favorite(u32 uVehicleId)
{
   if ( ! s_bFavoritesLoaded )
      load_favorites();

   if ( ( uVehicleId == 0 ) || (uVehicleId == MAX_U32) )
      return false;
   if ( s_iFavoritesCount >= MAX_FAVORITES )
      return false;

   s_uListFavorites[s_iFavoritesCount] = uVehicleId;
   s_iFavoritesCount++;
   save_favorites();
   return true;
}

u32 get_next_favorite(u32 uVehicleId)
{
   if ( ! s_bFavoritesLoaded )
      load_favorites();

   for( int i=0; i<s_iFavoritesCount; i++ )
   {
       if ( s_uListFavorites[i] != uVehicleId )
          continue;
       // We are now on the current favorite vehicle.
       // Go to next one
       i++;
       if ( i >= s_iFavoritesCount )
          i = 0;
       if ( 0 != s_uListFavorites[i] )
          return s_uListFavorites[i];
       else
          return uVehicleId;
   }
   return uVehicleId;
}
