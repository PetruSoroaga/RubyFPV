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

#include "../base/base.h"
#include <ctype.h>
#include "strings_loc.h"
#include "strings_table.h"

static const char* s_szLanguages[] = { "Chinese", "English", "French", "German", "Hindi", "Russian", "Spanish" };
static const char* s_szStringTableEmptyText = "";
static int s_iActiveLanguage = 0;
static int s_iLocalizationInited = 0;

int getLanguagesCount()
{
   return sizeof(s_szLanguages)/sizeof(s_szLanguages[0]);
}

const char* getLanguageName(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= getLanguagesCount()) )
      return s_szStringTableEmptyText;
   return s_szLanguages[iIndex];
}

void setActiveLanguage(int iLanguage)
{
   s_iActiveLanguage = iLanguage;
}

u32 _loc_string_compute_hash(const char* szString)
{
   if ( (NULL == szString) || (0 == szString[0]) )
      return MAX_U32;
   return 0;
}

void initLocalizationData()
{
   s_iLocalizationInited = 1;
   log_line("Initializing localization data for %d strings for %d languages", string_get_table_size(), getLanguagesCount());
}

const char* L(const char* szString)
{
   if ( (s_iActiveLanguage == 1) || (!s_iLocalizationInited) )
      return szString;

   if ( (NULL == szString) || (0 == szString[0] ) )
      return s_szStringTableEmptyText;

   type_localized_strings* pStringsTable = string_get_table();
   const char* pLocalized = NULL;
   for( int i=0; i<string_get_table_size(); i++ )
   {
      if ( 0 == strcmp(szString, pStringsTable[i].szEnglish) )
      {
          if (0 == s_iActiveLanguage )
             pLocalized = pStringsTable[i].szTranslatedCN;
          if (2 == s_iActiveLanguage )
             pLocalized = pStringsTable[i].szTranslatedFR;
          if (3 == s_iActiveLanguage )
             pLocalized = pStringsTable[i].szTranslatedDE;
          if (4 == s_iActiveLanguage )
             pLocalized = pStringsTable[i].szTranslatedHI;
          if (5 == s_iActiveLanguage )
             pLocalized = pStringsTable[i].szTranslatedRU;
          if (6 == s_iActiveLanguage )
             pLocalized = pStringsTable[i].szTranslatedSP;
          break;
      }
   }

   if ( (NULL != pLocalized) && (0 != *pLocalized) )
      return pLocalized;
   return szString;
}
