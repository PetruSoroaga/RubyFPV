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
#include "strings_table.h"

static const char* s_szLanguages[] = { "English", "Spanish", "French" };
static const char* s_szStringTableEmptyText = "";
static int s_iActiveLanguage = 0;
static int s_iLocalizationInited = 0;

typedef struct 
{
   const char* szEnglish;
   const char* szTranslated;
   u32 uHash;
} type_localize_string;

type_localize_string s_ListFrenchTexts[] = 
{
  {"Yes", "Oui", 0},
  {"No", "No", 0}
};

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
}

const char* L(const char* szString)
{
   if ( (s_iActiveLanguage == 0) || (!s_iLocalizationInited) )
      return szString;

   if ( (NULL == szString) || (0 == szString[0] ) )
      return s_szStringTableEmptyText;

   type_localize_string* pArray = NULL;

   if ( s_iActiveLanguage == 1 )
      pArray = s_ListFrenchTexts;

   if ( NULL == pArray )
      return szString;

   /*
   for( int i=0; i<sizeof(pArray); i++ )
   {
      if ( 0 == strcmp(szString, pArray[i].szEnglish) )
         return pArray[i].szTranslated;
   }
   */
   return szString;
}