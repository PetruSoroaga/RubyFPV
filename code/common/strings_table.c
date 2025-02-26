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

static const char* s_szStringTableUnknown = "[Missing Text]";
static const char* s_szStringTableEmptyText = "";
static const char* s_szStringTableEN[] =
{
// 0
"Firmware Instalation",
"Your controller needs to be fully flashed with the latest version of Ruby.",
"Your vehicle needs to be fully flashed with the latest version of Ruby. An OTA update is not sufficient.",
"Instead of a regular OTA update, a full firmware instalation is required as there where changes in latest Ruby that require a complete update of the system.",
"Computed based on current vehicle radio Tx power settings",
// 5
"Baseline",
"Main",
"Extended",
"High",
"High10",
// 10
"High422",
"High444",
"High10Intra",
"High422Intra",
"High444Intra",
// 15
"CAVL444Intra",
"Constrained Baseline",
"Constrained Main",
"Constrained Extended",
"Scalable Baseline",
// 20
"Scalable High",
"Your vehicle will not function properly until you do a full firmware flash on it."
};

const char* getString(u32 uStringId)
{
   if ( uStringId >= sizeof(s_szStringTableEN)/sizeof(s_szStringTableEN[0]) )
      return s_szStringTableUnknown;

   return s_szStringTableEN[uStringId];
}

const char* L(const char* szString)
{
   if ( (NULL == szString) || (0 == szString[0] ) )
      return s_szStringTableEmptyText;

   for( int i=0; i<sizeof(s_szStringTableEN)/sizeof(s_szStringTableEN[0]); i++ )
   {
      if ( 0 == strcmp(szString, s_szStringTableEN[i]) )
         return s_szStringTableEN[i];
   }
   return szString;
}